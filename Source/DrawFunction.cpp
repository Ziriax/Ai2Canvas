// DrawFunction.cpp
//
// Copyright (c) 2010-2014 Mike Swanson (http://blog.mikeswanson.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "IllustratorSDK.h"
#include "DrawFunction.h"
#include "IndentableStream.h"

using namespace CanvasExport;

DrawFunction::DrawFunction()
{
	// Initialize Function
	this->type = Function::kDrawFunction;

	// Initialize DrawFunction
	this->requestedName = "";
	this->hasGradients = false;
	this->hasPatterns = false;
	this->hasAlpha = false;
	this->followOrientation = 0.0f;
	this->rasterizeFileName = "";
	this->crop = false;
}

DrawFunction::~DrawFunction()
{
}

void DrawFunction::RenderDrawFunctionCall(const AIRealRect& documentBounds)
{
	// New line
	outFile << endl;
	{
		// No animation

		// Do we need to reposition?
		if (translateOrigin)
		{
			// Save drawing context state
			outFile << canvas->contextName << ".save();" << endl;

			Reposition(documentBounds);
		}

		// Just call the function
		outFile << name << "(" << canvas->contextName << ");" << endl;

		// Show origin
		if (debug)
		{
			outFile << canvas->contextName << ".save();" << endl;
			outFile << canvas->contextName << ".fillStyle = \"rgb(0, 0, 255)\";" << endl;
			outFile << canvas->contextName << ".fillRect(-2.0, -2.0, 5, 5);" << endl;
			outFile << canvas->contextName << ".restore();" << endl;
		}

		// Do we need to restore?
		if (translateOrigin)
		{
			// Restore drawing context state
			outFile << canvas->contextName << ".restore();" << endl;
		}
	}
}

// Render a drawing function
void DrawFunction::RenderDrawFunction(const AIRealRect& documentBounds)
{
	outFile << "export const " << name << " = {" << endl;
	{
		Indentation export_indentation(outFile);

		// Layer bounds
		outFile << "bounds: "
			<< "{ left: " << fixed << bounds.left
			<< ", top: " << fixed << bounds.bottom
			<< ", width: " << fixed << bounds.right - bounds.left
			<< ", height: " << fixed << bounds.top - bounds.bottom
			<< " }, " << endl;

		// Painter function
		outFile << "paint: (ctx: CanvasRenderingContext2D) => {" << endl;
		{
			Indentation paint_indentation(outFile);

			//// Need a blank line?
			//if (hasAlpha || hasGradients || hasPatterns)
			//{
			//	outFile << endl;
			//}

			// Does this draw function have alpha changes?
			if (hasAlpha)
			{
				// Grab the alpha value (so we can use it to compute new globalAlpha values during this draw function)
				outFile << "var alpha = ctx.globalAlpha;" << endl;
			}

			// Will we be encountering gradients?
			if (hasGradients)
			{
				outFile << "var gradient: CanvasGradient;" << endl;
			}

			// Will we be encountering patterns?
			if (hasPatterns)
			{
				outFile << "var pattern: CanvasPattern;" << endl;
			}

			/// Re-set matrix based on document
			sAIRealMath->AIRealMatrixSetIdentity(&canvas->currentState->internalTransform);
			sAIRealMath->AIRealMatrixConcatScale(&canvas->currentState->internalTransform, 1, -1);
			sAIRealMath->AIRealMatrixConcatTranslate(&canvas->currentState->internalTransform, -1 * documentBounds.left, documentBounds.top);

			// Do we need to move the origin?
			if (translateOrigin)
			{
				// Calculate offsets to move function/layer to 0, 0 of document
				AIReal offsetH = bounds.left - documentBounds.left;
				AIReal offsetV = bounds.top - documentBounds.top;

				// Calculate requested offsets based on percentages
				AIReal translateH = (bounds.right - bounds.left) * translateOriginH;
				AIReal translateV = (bounds.top - bounds.bottom) * translateOriginV;

				// Modify transformation matrix for this function (and set of layers)
				sAIRealMath->AIRealMatrixConcatTranslate(&canvas->currentState->internalTransform, (-1 * offsetH) - translateH, offsetV - translateV);
			}

			// Are we supposed to rasterize this function?
			if (!rasterizeFileName.empty())
			{
				// Rasterize the first layer
				// TODO: Note that this only rasterizes the first associated layer. What if this has multiple layers?

				// Output layer name
				outFile << "// " << name;

				canvas->RenderUnsupportedArt(layers[0]->artHandle, rasterizeFileName, 1);
			}
			else
			{
				// Render each layer in the function block (they're already in the correct order)
				for (unsigned int i = 0; i < layers.size(); i++)
				{
					// Render the art
					canvas->RenderArt(layers[i]->artHandle, 1);

					// Restore remaining state
					canvas->SetContextDrawingState(1);
				}
			}
		}
		outFile << "}" << endl;
	}

	outFile << "};" << endl;
}

// Output repositioning translation for a draw function
void DrawFunction::Reposition(const AIRealRect& documentBounds)
{
	// Calculate offsets to move function/layer to 0, 0 of document
	AIReal offsetH = bounds.left - documentBounds.left;
	AIReal offsetV = bounds.top - documentBounds.top;

	// Calculate requested offsets based on percentages
	AIReal translateH = (bounds.right - bounds.left) * this->translateOriginH;
	AIReal translateV = (bounds.top - bounds.bottom) * this->translateOriginV;

	// Re-positioning translation
	AIReal x = offsetH + translateH;
	AIReal y = (-1 * offsetV) + translateV;

	// Render the repositioning translation for this function
	// NOTE: This needs to happen, even if it's just "identity," since other functions may have already changed the transformation
	outFile << canvas->contextName << ".translate(" << setiosflags(ios::fixed) << setprecision(1) << x << ", " << y << ");" << endl;
}

void DrawFunction::SetParameter(const std::string& parameter, const std::string& value)
{
	// Origin
	if (parameter == "origin" ||
		parameter == "o")
	{
		if (debug)
		{
			outFile << "//     Found origin parameter" << endl;
		}

		// Short-cut values?
		if (value == "normal" ||
			value == "n")
		{
			// Normal (do nothing special)
			this->translateOrigin = false;
		}
		else if (value == "center" ||
			value == "c")
		{
			// Center
			this->translateOrigin = true;
			this->translateOriginH = 0.5f;
			this->translateOriginV = 0.5f;
		}
		else if (value == "upper-left" ||
			value == "ul")
		{
			// Upper left
			this->translateOrigin = true;
			this->translateOriginH = 0.0f;
			this->translateOriginV = 0.0f;
		}
		else if (value == "upper-right" ||
			value == "ur")
		{
			// Upper right
			this->translateOrigin = true;
			this->translateOriginH = 1.0f;
			this->translateOriginV = 0.0f;
		}
		else if (value == "lower-right" ||
			value == "lr")
		{
			// Lower right
			this->translateOrigin = true;
			this->translateOriginH = 1.0f;
			this->translateOriginV = 1.0f;
		}
		else if (value == "lower-left" ||
			value == "ll")
		{
			// Lower left
			this->translateOrigin = true;
			this->translateOriginH = 0.0f;
			this->translateOriginV = 1.0f;
		}
		else
		{
			// Can we parse two float values?
			std::vector<std::string> originOffsets = Tokenize(value, ",");

			// Did we find two values?
			if (originOffsets.size() == 2)
			{
				// Indicate that we want to translate
				this->translateOrigin = true;

				// Set translation values
				this->translateOriginH = (AIReal)strtod(originOffsets[0].c_str(), NULL);
				this->translateOriginV = (AIReal)strtod(originOffsets[1].c_str(), NULL);

				if (debug)
				{
					outFile << "//     translateH = " << setiosflags(ios::fixed) << setprecision(1) <<
						this->translateOriginH << ", translateV = " << this->translateOriginV << endl;
				}
			}
		}
	}

	// Rasterize
	if (parameter == "rasterize" ||
		parameter == "rast")
	{
		if (debug)
		{
			outFile << "//     Found rasterize parameter" << endl;
		}

		if (value == "no" ||
			value == "n")
		{
			// No rasterization
			this->rasterizeFileName = "";
		}
		else
		{
			// If we have a value
			if (!value.empty())
			{
				std::string fileName = value;

				// Does the file name already have an extension?
				size_t index = value.find_last_of('.');
				if (index != string::npos)
				{
					// Remove the extension
					fileName = name.substr(0, index);
				}

				// Since we'll rasterize to a PNG file, add a ".png" extension
				fileName += ".png";

				// Store file name
				this->rasterizeFileName = fileName;

				if (debug)
				{
					outFile << "//     Rasterize file name = " << fileName << endl;
				}
			}
		}
	}

	// Crop
	if (parameter == "crop" ||
		parameter == "c")
	{
		if (debug)
		{
			outFile << "//     Found crop parameter" << endl;
		}

		if (value == "yes" ||
			value == "y")
		{
			// Crop
			this->crop = true;

			// Set first layer to crop
			// TODO: This only works on the first layer
			this->layers[0]->crop = true;
		}
		else if (value == "no" ||
			value == "n")
		{
			// Don't crop
			this->crop = false;

			// Set first layer to NOT crop
			// TODO: This only works on the first layer
			this->layers[0]->crop = false;
		}
	}
}
