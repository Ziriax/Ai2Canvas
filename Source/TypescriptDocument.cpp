// JavascriptDocument.cpp
//
// Copyright (c) 2010-2014 Mike Swanson (http://blog.mikeswanson.com)
// Copyright (c) 2018- Peter Verswyvelen (http://github.com/Ziriax)
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
#include "TypescriptDocument.h"
#include "IndentableStream.h"

// Current plug-in version
#define PLUGIN_VERSION "1.4"

using namespace CanvasExport;

TypescriptDocument::TypescriptDocument(const std::string& pathName)
{
	// Initialize Document
	this->mainCanvas = NULL;
	this->fileName = "";

	// Parse the folder path
	ParseFolderPath(pathName);

	// Add a canvas for the primary document
	this->mainCanvas = canvases.Add("canvas", "ctx", &resources);
}

TypescriptDocument::~TypescriptDocument()
{
	// Clear layers
	for (unsigned int i = 0; i < layers.size(); i++)
	{
		// Remove instance
		delete layers[i];
	}
}

void TypescriptDocument::Render()
{
	// Scan the document for layers and layer attributes
	ScanDocument();

	// Parse the layers
	ParseLayers();

	// Render the document
	RenderDocument();
}

// Set the bounds for the primary document
void TypescriptDocument::SetDocumentBounds()
{
	// Set default bounds
	// Start with maximums and minimums, so any value will cause them to be set
	documentBounds.left = FLT_MAX;
	documentBounds.right = -FLT_MAX;
	documentBounds.top = -FLT_MAX;
	documentBounds.bottom = FLT_MAX;

	// Loop through all layers
	for (unsigned int i = 0; i < layers.size(); i++)
	{
		// Does this layer crop the entire canvas?
		if (layers[i]->crop)
		{
			// This layer's bounds crop the entire canvas
			documentBounds = layers[i]->bounds;

			// No need to look through any more layers
			break;
		}
		else
		{
			// Update with layer bounds
			UpdateBounds(layers[i]->bounds, documentBounds);
		}
	}

	// Set canvas size
	mainCanvas->width = documentBounds.right - documentBounds.left;
	mainCanvas->height = documentBounds.top - documentBounds.bottom;
}

// Find the base folder path and filename
void TypescriptDocument::ParseFolderPath(const std::string& pathName)
{
	// Construct a FilePath
	ai::UnicodeString usPathName(pathName);
	ai::FilePath aiFilePath(usPathName);

	// Extract folder and file names
	resources.folderPath = aiFilePath.GetDirectory(false).as_Platform();
	fileName = aiFilePath.GetFileNameNoExt().as_Platform();
}

// Parse the layers
void TypescriptDocument::ParseLayers()
{
	// Loop through all layers
	for (unsigned int i = 0; i < layers.size(); i++)
	{
		// Parse the layer name (and grab options)
		std::string name;
		std::string optionValue;
		ParseLayerName(*layers[i], name, optionValue);

		// Tokenize the options
		std::vector<std::string> options = Tokenize(optionValue, ";");

		Function* function = NULL;
		{
			// This is a draw function
			// TODO: Fix weird cast.
			DrawFunction* drawFunction = functions.AddDrawFunction(name);

			// Add this layer to the function
			drawFunction->layers.push_back(layers[i]);

			// Track whether this function will be drawing with alpha, a gradient, or a pattern
			drawFunction->hasAlpha |= layers[i]->hasAlpha;
			drawFunction->hasGradients |= layers[i]->hasGradients;
			drawFunction->hasPatterns |= layers[i]->hasPatterns;

			// Set canvas
			drawFunction->canvas = mainCanvas;

			// Set pointer
			function = drawFunction;
		}

		// Update to include new layer bounds
		UpdateBounds(layers[i]->bounds, function->bounds);

		// Set function options
		SetFunctionOptions(options, *function);
	}
}

// Parses an individual layer name/options
void TypescriptDocument::ParseLayerName(const Layer& layer, std::string& name, std::string& optionValue)
{
	// We may need to modify the functionName
	AIBoolean hasFunctionName = false;
	name = layer.name;

	// Does the layer name end with a function syntax?
	// "a();"

	// Is the name long enough to contain function syntax?
	size_t length = name.length();
	if (length > 3)
	{
		// Does the end of the layer name contain the function syntax?
		if (name.substr(length - 2) == ");")
		{
			// Find the opening parenthesis
			size_t index = name.find_last_of('(');

			// Did we find the parenthesis?
			if (index != string::npos)
			{
				// Copy options
				optionValue = name.substr((index + 1), (length - index - 3));

				if (debug)
				{
					outFile << "//   Found options = " << optionValue << endl;
				}

				// Terminate the layer name starting at the opening parenthesis
				name.erase(index);

				// Note that we found a function name
				hasFunctionName = true;
			}
		}
	}

	// Do we have a specified function name?
	if (!hasFunctionName)
	{
		// Construct default function name
		name = layer.name;
	}

	// Convert to camel-case
	CleanString(name, true);
}

// Render the document
void TypescriptDocument::RenderDocument()
{
	// Set document bounds
	SetDocumentBounds();

	// Output document bounds
	auto& bounds = documentBounds;
	outFile << "export const bounds = "
		<< "{ left: " << fixed << bounds.left
		<< ", top: " << fixed << bounds.bottom
		<< ", width: " << fixed << bounds.right - bounds.left
		<< ", height: " << fixed << bounds.top - bounds.bottom
		<< "  }; " << endl;

	outFile << endl;

	// Render the symbol functions
	RenderSymbolFunctions();

	// Render the pattern function
	RenderPatternFunction();


	// Do we need a pattern function?
	if (mainCanvas->documentResources->patterns.HasPatterns())
	{
		outFile << "drawPatterns();";
	}

	// Render the functions/layers
	functions.RenderDrawFunctions(documentBounds);
}

// Set the options for a draw or animation function
void TypescriptDocument::SetFunctionOptions(const std::vector<std::string>& options, Function& function)
{
	// Loop through options
	for (unsigned int i = 0; i < options.size(); i++)
	{
		// Split the parameter and value
		std::vector<std::string> split = Tokenize(options[i], ":");

		// Did we get two values?
		if (split.size() == 2)
		{
			// Clean the parameter name
			std::string parameter = split[0];
			CleanParameter(parameter);
			ToLower(parameter);

			// Clean the parameter value
			std::string value = split[1];
			CleanParameter(value);

			// Process options based on type
			// TODO: Can we make this more OO?
			switch (function.type)
			{
			case Function::kDrawFunction:
			{
				DrawFunction& drawFunction = (DrawFunction&)function;

				// Set draw function parameter
				drawFunction.SetParameter(parameter, value);
			}
			break;
			case Function::kAnyFunction:
			{
			}
			break;
			}
		}
	}
}

// Scan all visible elements in the art tree
//   Track maximum bounds of visible artwork per layer
//   Track pattern fills used by visible artwork
//   Track if gradient fills are used by visible artwork per layer
void TypescriptDocument::ScanDocument()
{
	AILayerHandle layerHandle = NULL;
	ai::int32 layerCount = 0;

	// How many layers in this document?
	sAILayer->CountLayers(&layerCount);

	// Loop through all layers backwards
	// We loop backwards, since the HTML5 canvas element uses a "painter model"
	for (long i = (layerCount - 1); i > -1; i--)
	{
		// Get a reference to the layer
		sAILayer->GetNthLayer(i, &layerHandle);

		// Is the layer visible?
		AIBoolean isLayerVisible = false;
		sAILayer->GetLayerVisible(layerHandle, &isLayerVisible);
		if (debug)
		{
			outFile << "// Layer visible = " << isLayerVisible << endl;
		}

		// Only process if the layer is visible
		if (isLayerVisible)
		{
			// Add this layer
			Layer* layer = AddLayer(layers, layerHandle);

			// Scan this layer
			ScanLayer(*layer);
		}
	}
}

void TypescriptDocument::ScanLayer(Layer& layer)
{
	// Get the first art in this layer
	AIArtHandle artHandle = NULL;
	sAIArt->GetFirstArtOfLayer(layer.layerHandle, &artHandle);

	// Remember artwork handle
	layer.artHandle = artHandle;

	// Scan the artwork tree and capture layer data
	ScanLayerArtwork(layer.artHandle, 1, layer);
}

// Scans a layer's artwork tree to capture important data
void TypescriptDocument::ScanLayerArtwork(AIArtHandle artHandle, unsigned int depth, Layer& layer)
{
	// Loop through all artwork at this depth
	do
	{
		// Is this art visible?
		AIBoolean isArtVisible = false;
		ai::int32 attr = 0;
		sAIArt->GetArtUserAttr(artHandle, kArtHidden, &attr);
		isArtVisible = !((attr &kArtHidden) == kArtHidden);

		// Only consider if art is visible
		if (isArtVisible)
		{
			// Get the art bounds
			AIRealRect artBounds;
			sAIArt->GetArtBounds(artHandle, &artBounds);

			// Update the bounds
			UpdateBounds(artBounds, layer.bounds);

			// Get type
			short type = 0;
			sAIArt->GetArtType(artHandle, &type);

			// Is this symbol art?
			if (type == kSymbolArt)
			{
				// Get the symbol pattern
				AIPatternHandle symbolPatternHandle = nil;
				sAISymbol->GetSymbolPatternOfSymbolArt(artHandle, &symbolPatternHandle);

				// Add the symbol pattern
				bool added = mainCanvas->documentResources->patterns.Add(symbolPatternHandle, true);

				// If we added a new pattern, scan its artwork
				if (added)
				{
					AIArtHandle patternArtHandle = NULL;
					sAIPattern->GetPatternArt(symbolPatternHandle, &patternArtHandle);

					// Look inside, but don't screw up bounds for our current layer
					Layer symbolLayer;
					ScanLayerArtwork(patternArtHandle, (depth + 1), symbolLayer);

					// Capture features for pattern
					Pattern* pattern = mainCanvas->documentResources->patterns.Find(symbolPatternHandle);
					pattern->hasGradients = symbolLayer.hasGradients;
					pattern->hasPatterns = symbolLayer.hasPatterns;		// Can this ever happen?
					pattern->hasAlpha = symbolLayer.hasAlpha;
				}
			}
			else if (type == kPluginArt)
			{
				// Get the result art handle
				AIArtHandle resultArtHandle = NULL;
				sAIPluginGroup->GetPluginArtResultArt(artHandle, &resultArtHandle);

				// Get the first art element in the result group
				AIArtHandle childArtHandle = NULL;
				sAIArt->GetArtFirstChild(resultArtHandle, &childArtHandle);

				// Look inside the result group
				ScanLayerArtwork(childArtHandle, (depth + 1), layer);
			}

			// Get opacity
			AIReal opacity = sAIBlendStyle->GetOpacity(artHandle);
			if (opacity != 1.0f)
			{
				// Flag that this layer includes alpha/opacity changes
				layer.hasAlpha = true;
			}

			// Get the style for this artwork
			AIPathStyle style;
			sAIPathStyle->GetPathStyle(artHandle, &style);

			// Does this artwork use a pattern fill or a gradient?
			if (style.fillPaint)
			{
				switch (style.fill.color.kind)
				{
				case kPattern:
				{
					// Add the pattern
					mainCanvas->documentResources->patterns.Add(style.fill.color.c.p.pattern, false);

					// Flag that this layer includes patterns
					layer.hasPatterns = true;
					break;
				}
				case kGradient:
				{
					// Flag that this layer includes gradients
					layer.hasGradients = true;
					break;
				}
				case kGrayColor:
				case kFourColor:
				case kCustomColor:
				case kThreeColor:
				case kNoneColor:
				{
					break;
				}
				}
			}

			// Does this artwork use a pattern stroke?
			if (style.strokePaint)
			{
				switch (style.stroke.color.kind)
				{
				case kPattern:
				{
					// Add the pattern
					mainCanvas->documentResources->patterns.Add(style.stroke.color.c.p.pattern, false);

					// Flag that this layer includes patterns
					layer.hasPatterns = true;
					break;
				}
				case kGradient:
				{
					// Flag that this layer includes gradients
					layer.hasGradients = true;
					break;
				}
				case kGrayColor:
				case kFourColor:
				case kCustomColor:
				case kThreeColor:
				case kNoneColor:
				{
					break;
				}
				}
			}

			// See if this artwork has any children
			AIArtHandle childArtHandle = NULL;
			sAIArt->GetArtFirstChild(artHandle, &childArtHandle);

			// Did we find anything?
			if (childArtHandle)
			{
				// Scan artwork at the next depth
				ScanLayerArtwork(childArtHandle, (depth + 1), layer);
			}
		}

		// Find the next sibling
		sAIArt->GetArtSibling(artHandle, &artHandle);
	} while (artHandle != NULL);
}

void TypescriptDocument::RenderSymbolFunctions()
{
	// Do we have symbol functions to render?
	if (mainCanvas->documentResources->patterns.HasSymbols())
	{
		// Loop through symbols
		for (unsigned int i = 0; i < mainCanvas->documentResources->patterns.Patterns().size(); i++)
		{
			// Is this a symbol?
			if (mainCanvas->documentResources->patterns.Patterns()[i]->isSymbol)
			{
				// Pointer to pattern (for convenience)
				Pattern* pattern = mainCanvas->documentResources->patterns.Patterns()[i];

				// Begin symbol function block
				outFile << "function " << pattern->name << "(ctx: CanvasRenderingContext2D) {" << endl;
				{
					Indentation indentation(outFile);

					// Need a blank line?
					//if (pattern->hasAlpha || pattern->hasGradients || pattern->hasPatterns)
					//{
					//	outFile << endl;
					//}

					// Does this draw function have alpha changes?
					if (pattern->hasAlpha)
					{
						// Grab the alpha value (so we can use it to compute new globalAlpha values during this draw function)
						outFile << "const alpha = ctx.globalAlpha;" << endl;
					}

					// Will we be encountering gradients?
					if (pattern->hasGradients)
					{
						outFile << "var gradient: CanvasGradient;" << endl;
					}

					// Will we be encountering patterns?
					// TODO: Is this even possible?
					if (pattern->hasPatterns)
					{
						outFile << "var pattern: CanvasPattern;" << endl;
					}

					// Get a handle to the pattern art
					AIArtHandle patternArtHandle = nil;
					sAIPattern->GetPatternArt(pattern->patternHandle, &patternArtHandle);

					// While we're here, get the size of this canvas
					AIRealRect bounds;
					sAIArt->GetArtBounds(patternArtHandle, &bounds);
					if (debug)
					{
						outFile << "// Symbol art bounds = " <<
							"left:" << setiosflags(ios::fixed) << setprecision(1) << bounds.left <<
							", top:" << bounds.top <<
							", right:" << bounds.right <<
							", bottom:" << bounds.bottom << endl;
					}

					// Create canvas and set size
					Canvas* canvas = new Canvas("canvas", &resources);			// No need to add it to the collection, since it doesn't represent a canvas element
					canvas->contextName = "ctx";
					canvas->width = bounds.right - bounds.left;
					canvas->height = bounds.top - bounds.bottom;
					canvas->currentState->isProcessingSymbol = true;

					// Get the first art element in the symbol
					AIArtHandle childArtHandle = NULL;
					sAIArt->GetArtFirstChild(patternArtHandle, &childArtHandle);

					// Render this sub-group
					canvas->RenderArt(childArtHandle, 1);

					// Restore remaining state
					canvas->SetContextDrawingState(1);

					// Free the canvas
					delete canvas;
				}

				// End function block
				outFile << "}" << endl;
			}
		}
	}
}

void TypescriptDocument::RenderPatternFunction()
{
	// Do we have pattern functions to render?
	if (mainCanvas->documentResources->patterns.HasPatterns())
	{
		// Begin pattern function block
		outFile << "function drawPatterns() {" << endl;
		{
			Indentation indentation(outFile);
			 
			// Loop through patterns
			for (unsigned int i = 0; i < mainCanvas->documentResources->patterns.Patterns().size(); i++)
			{
				// Is this a pattern?
				if (!mainCanvas->documentResources->patterns.Patterns()[i]->isSymbol)
				{
					// Allocate space for pattern name
					ai::UnicodeString patternName;

					// Pointer to pattern (for convenience)
					Pattern* pattern = mainCanvas->documentResources->patterns.Patterns()[i];

					// Get pattern name
					sAIPattern->GetPatternName(pattern->patternHandle, patternName);
					if (debug)
					{
						outFile << "//   Pattern name = " << patternName.as_Platform() << " (" << pattern->patternHandle << ")" << endl;
					}

					// Create canvas ID
					std::ostringstream canvasID;
					canvasID << "pattern" << pattern->canvasIndex;

					// Create context name
					std::ostringstream contextName;
					contextName << "ctx" << pattern->canvasIndex;

					// Create canvas for this pattern
					Canvas* canvas = canvases.Add(canvasID.str(), contextName.str(), &resources);
					canvas->isHidden = true;
					canvas->currentState->isProcessingSymbol = false;

					// Render context commands
					outFile << "const " << canvas->id << " = document.getElementById(\"" << canvas->id << "\");" << endl;
					outFile << "const " << canvas->contextName << " = " << canvas->id << ".getContext(\"2d\");" << endl;

					// Get a handle to the pattern art
					AIArtHandle patternArtHandle = nil;
					sAIPattern->GetPatternArt(pattern->patternHandle, &patternArtHandle);

					// While we're here, get the size of this canvas
					AIRealRect bounds;
					sAIArt->GetArtBounds(patternArtHandle, &bounds);
					if (debug)
					{
						outFile << "// Symbol art bounds = " <<
							"left:" << setiosflags(ios::fixed) << setprecision(1) << bounds.left <<
							", top:" << bounds.top <<
							", right:" << bounds.right <<
							", bottom:" << bounds.bottom << endl;
					}

					// Set canvas size
					canvas->width = bounds.right - bounds.left;
					canvas->height = bounds.top - bounds.bottom;

					// If this isn't a symbol, modify the transformation
					if (!pattern->isSymbol)
					{
						// Set internal transform
						// TODO: While this works, it seems awfully convoluted
						sAIRealMath->AIRealMatrixSetIdentity(&canvas->currentState->internalTransform);
						sAIRealMath->AIRealMatrixConcatScale(&canvas->currentState->internalTransform, 1, -1);
						sAIRealMath->AIRealMatrixConcatTranslate(&canvas->currentState->internalTransform, -1 * bounds.left, bounds.top);
						sAIRealMath->AIRealMatrixConcatScale(&canvas->currentState->internalTransform, 1, -1);
						sAIRealMath->AIRealMatrixConcatTranslate(&canvas->currentState->internalTransform, 0, canvas->height);
					}

					// This canvas shound be hidden, since it's only used for the pattern artwork
					canvas->isHidden = true;

					// Get the first art element in the pattern
					AIArtHandle childArtHandle = NULL;
					sAIArt->GetArtFirstChild(patternArtHandle, &childArtHandle);

					// Render this sub-group
					canvas->RenderArt(childArtHandle, 1);

					// Restore remaining state
					canvas->SetContextDrawingState(1);
				}
			}
		}

		// End function block
		outFile << "}" << endl;
	}
}

void TypescriptDocument::DebugInfo()
{
	outFile << "<p>This document has been exported in debug mode.</p>" << endl;

	resources.images.DebugInfo();

	functions.DebugInfo();
}
