// Document.cpp
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
#include "HtmlDocument.h"

// Current plug-in version
#define PLUGIN_VERSION "1.3"

using namespace CanvasExport;

HtmlDocument::HtmlDocument(const std::string& pathName)
{
	// Initialize Document
	this->canvas = NULL;
	this->fileName = "";
	this->hasAnimation = false;

	// Parse the folder path
	ParseFolderPath(pathName);

	// Add a canvas for the primary document
	this->canvas = canvases.Add("canvas", "ctx", &resources);
}

HtmlDocument::~HtmlDocument()
{
	// Clear layers
	for (unsigned int i = 0; i < layers.size(); i++)
	{
		// Remove instance
		delete layers[i];
	}
}

void HtmlDocument::Render()
{
	// Document type
	outFile << "<!DOCTYPE html>";

	// Header, version information, and contact details
	#ifdef MAC_ENV
		outFile << "<!-- Created with Ai->Canvas Export Plug-In Version " << PLUGIN_VERSION << " (Mac)   -->" << endl;
	#endif 
	#ifdef WIN_ENV
	#ifdef _WIN64
		outFile << "<!-- Created with Ai->Canvas Export Plug-In Version " << PLUGIN_VERSION << " (PC/64) -->" << endl;
	#else
		outFile << "<!-- Created with Ai->Canvas Export Plug-In Version " << PLUGIN_VERSION << " (PC/32) -->" << endl;
	#endif
	#endif 
	size_t length = std::string(PLUGIN_VERSION).length();
	std::string padding = std::string(length, ' ');
	outFile << "<!-- By Mike Swanson (http://blog.mikeswanson.com/)    " << padding << "      -->\n" << endl;

	// Output header information
	outFile << "<html lang=\"en\">" << endl;
	outFile << "<head>" << endl;
	outFile << "<meta charset=\"UTF-8\" />" << endl;

	// NOTE: The following meta tag is required when browsing HTML files using IE9 over an intranet
	//outFile << "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=9\" />" << endl;

	outFile << "<title>" << fileName << "</title>" << endl;

	if (debug)
	{
		outFile << "<!--" << endl;
	}

	if (debug)
	{
		WriteArtTree();
	}

	// Scan the document for layers and layer attributes
	ScanDocument();

	// Parse the layers
	ParseLayers();

	if (debug)
	{
		outFile << "-->\n" << endl;
	}

	// If we have animation, link to animation JavaScript support file
	if (hasAnimation)
	{
		// Create the animation support file (if it doesn't already exist)
		CreateAnimationFile();

		// Output a reference
		outFile << "<script src=\"Ai2CanvasAnimation.js\"></script>" << endl;
	}

	// Note that "type='text/javascript'" is no longer required as of HTML5, unless the language isn't javascript
	outFile << "<script>" << endl;
	
	// Render the document
	RenderDocument();

	// Close script tag
	outFile << "</script>" << endl;

	// Debug style information
	if (debug)
	{
		outFile << "<style type=\"text/css\">" << endl;
		outFile << "body {" << endl;
		outFile << "font-family: Verdana, Geneva, sans-serif;" << endl;
		outFile << "font-size: 12px;" << endl;
		outFile << "}" << endl;
		outFile << "canvas {" << endl;
		outFile << "border: 1px solid grey;" << endl;
		outFile << "}" << endl;
		outFile << "</style>" << endl;
	}

	// End of header
	outFile << "</head>" << endl;

	// Body header with onLoad init function to execute
	outFile << "<body onload=\"init()\">" << endl;

	// Render canvases
	canvases.Render();

	// Render images
	resources.images.Render();

	// Include basic debug info
	if (debug)
	{
		DebugInfo();
	}

	// Body end
	outFile << "</body>" << endl;

	// End of document
	outFile << "</html>" << endl;
}

// Set the bounds for the primary document
void HtmlDocument::SetDocumentBounds()
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
	canvas->width = documentBounds.right - documentBounds.left;
	canvas->height = documentBounds.top - documentBounds.bottom;
}

// Find the base folder path and filename
void HtmlDocument::ParseFolderPath(const std::string& pathName)
{
	// Construct a FilePath
	ai::UnicodeString usPathName(pathName);
	ai::FilePath aiFilePath(usPathName);

	// Extract folder and file names
	resources.folderPath = aiFilePath.GetDirectory(false).as_Platform();
	fileName = aiFilePath.GetFileNameNoExt().as_Platform();
}

// Parse the layers
void HtmlDocument::ParseLayers()
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

		// Is this an animation?
		if (HasAnimationOption(options))
		{
			// Create a new animation function (will automatically make the function name unique, if necessary)
			// TODO: Fix weird cast.
			AnimationFunction* animationFunction = functions.AddAnimationFunction(name);

			// Set values
			animationFunction->artHandle = layers[i]->artHandle;
			animationFunction->canvas = canvas;

			// Set pointer
			function = animationFunction;

			// Note that this document has animations
			hasAnimation = true;
		}
		else
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
			drawFunction->canvas = canvas;

			// Set pointer
			function = drawFunction;
		}

		// Update to include new layer bounds
		UpdateBounds(layers[i]->bounds, function->bounds);

		// Set function options
		SetFunctionOptions(options, *function);
	}

	// Bind string animation funtion names to actual animation function objects
	functions.BindAnimationFunctions();

	// Bind triggers
	functions.BindTriggers();
}

bool HtmlDocument::HasAnimationOption(const std::vector<std::string>& options)
{
	bool result = false;

	// Loop through all options
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
			ToLower(value);

			// Type?
			if (parameter == "type" ||
				parameter == "t")
			{
				// Is this an animation type?
				if (value == "animation" ||
					value == "a")
				{
					// Is an animation
					result = true;
					break;
				}
				else if (value == "drawing" ||
						 value == "d")
				{
					// Not an animation
					result = false;
					break;
				}
			}
		}
	}

	return result;
}

// Parses an individual layer name/options
void HtmlDocument::ParseLayerName(const Layer& layer, std::string& name, std::string& optionValue)
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

				// Convert to camel-case
				CleanString(name, true);

				// Note that we found a function name
				hasFunctionName = true;
			}
		}
	}

	// Do we have a specified function name?
	if (!hasFunctionName)
	{
		// Assign a default function name
		name = "draw";
	}
}

// Render the document
void HtmlDocument::RenderDocument()
{
	// Set document bounds
	SetDocumentBounds();

	// For animation, create global canvas and context references
	if (hasAnimation)
	{
		outFile << "// Main canvas and context references" << endl;
		outFile << "var " << canvas->id << ";" << endl;
		outFile << "var " << canvas->contextName << ";" << endl;
	}

	// Render animations
	RenderAnimations();

	// Begin init function block
	outFile << "function init() {" << endl;

	// For animation, set main canvas and context references
	if (hasAnimation)
	{
		outFile << "// Set main canvas and context references" << endl;
		outFile  << canvas->id << " = document.getElementById(\"" << canvas->id << "\");" << endl;
		outFile  << canvas->contextName << " = " << canvas->id << ".getContext(\"2d\");" << endl;
	}

	// Do we need a pattern function?
	if (canvas->documentResources->patterns.HasPatterns())
	{
		outFile << "drawPatterns();" << endl;
	}

	// Any animations to set-up?
	if (hasAnimation)
	{
		if (debug)
		{
			// Use mouse movement to debug animation
			outFile << "// Capture mouse events for debug clock" << endl;
		    outFile  << canvas->id << ".addEventListener(\"click\", setDebugClock, false);" << endl;
		    outFile  << canvas->id << ".addEventListener(\"mousemove\", getMouseLocation, false);" << endl;
		}

		// Initialize animation clocks
		functions.RenderClockInit();

		// Start animation clocks
		functions.RenderClockStart();

		// Set animation timer
		outFile << "// Set animation timer" << endl;
		outFile << "setInterval(drawFrame, (1000 / fps));" << endl;
		outFile << "}" << endl;

		// Include "update animations"
		outFile << "function updateAnimations() {" << endl;

		// Tick animation clocks
		functions.RenderClockTick();

		outFile << "}" << endl;

		// Render drawFrame function
		outFile << "function drawFrame() {" << endl;
		outFile << "// Update animations" << endl;
		outFile << "updateAnimations();" << endl;
		outFile << "// Clear canvas" << endl;
		// The following method should clear the canvas, but it causes visual glitching with Safari and Chrome
		//outFile  << canvas->id << ".width = " << canvas->id << ".width;" << endl;
		outFile  << canvas->contextName << ".clearRect(0, 0, " << canvas->id << ".width, " << canvas->id << ".height);" << endl;

		// Draw function calls
		functions.RenderDrawFunctionCalls(documentBounds);

		// Debug Bezier curves
		if (debug && functions.HasAnimationFunctions())
		{
			// Debug linear animation
			outFile << "plotLinearPoints(" << canvas->contextName  << ");" << endl;
			outFile << "plotAnchorPoints(" << canvas->contextName  << ");" << endl;
		}

		// Debug animation fps
		if (debug)
		{
			outFile << "// Count actual fps" << endl;
			outFile << "++frameCount;" << endl;
			outFile << "var now = new Date().getTime();" << endl;
			outFile << "if (now > frameTime) {" << endl;
			outFile << "frameTime = now + 1000;" << endl;
			outFile << "frameReport = frameCount;" << endl;
			outFile << "frameCount = 0;" << endl;
			outFile << "}" << endl;
			outFile << "// Report debug information" << endl;
			outFile  << canvas->contextName << ".save();" << endl;
			outFile  << canvas->contextName << ".fillStyle = \"rgb(0, 0, 255)\";" << endl;
			outFile  << canvas->contextName << ".fillText(frameReport + \" fps\", 5, 10);" << endl;
			outFile  << canvas->contextName << ".fillText((debug.ticks() / 1000).toFixed(1) + \" / \" + debug.timeRange.toFixed(1) + \" s\", 5, 20);" << endl;
			outFile  << canvas->contextName << ".restore();" << endl;
		}

		// End function block
		outFile << "}" << endl;
	}
	else
	{
		// No animations
		outFile << "var " << canvas->id << " = document.getElementById(\"" << canvas->id << "\");" << endl;
		outFile << "var " << canvas->contextName << " = " << canvas->id << ".getContext(\"2d\");" << endl;

		// Draw function calls
		functions.RenderDrawFunctionCalls(documentBounds);

		// End function block
		outFile << "}" << endl;
	}

	// Render the functions/layers
	functions.RenderDrawFunctions(documentBounds);

	// Render the symbol functions
	RenderSymbolFunctions();

	// Render the pattern function
	RenderPatternFunction();
}

void HtmlDocument::RenderAnimations()
{
	// Is there any animation in this document (paths or rotation?)
	if (hasAnimation)
	{
		// Output frames per second
		outFile << "// Frames per second" << endl;
		outFile << "var fps = 60.0;" << endl;

		if (debug)
		{
			outFile << "var frameTime = 0;" << endl;
			outFile << "var frameCount = 0;" << endl;
			outFile << "var frameReport = 0;" << endl;
			outFile << "var debug = new debugClock();" << endl;
		}
	}

	// Initialize animations
	functions.RenderAnimationFunctionInits(documentBounds);

	if (debug)
	{
		if (functions.HasAnimationFunctions())
		{
			// Add linear animation debug JavaScript
			DebugAnimationPathJS();
		}

		if (hasAnimation)
		{
			// Add debug animation clock 
			DebugClockJS();
		}
	}
}

// Set the options for a draw or animation function
void HtmlDocument::SetFunctionOptions(const std::vector<std::string>& options, Function& function)
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

					// Any animation?
					if (!drawFunction.animationFunctionName.empty() ||
					   (drawFunction.rotateClock.direction != AnimationClock::kNone) ||
					   (drawFunction.scaleClock.direction != AnimationClock::kNone) ||
					   (drawFunction.alphaClock.direction != AnimationClock::kNone))
					{
						// Document has animation
						hasAnimation = true;
					}

					break;
				}
				case Function::kAnimationFunction:
				{
					// Set animation function parameter
					((AnimationFunction&)function).SetParameter(parameter, value);

					// Animation
					hasAnimation = true;

					break;
				}
                case Function::kAnyFunction:
                {
                    break;
                }
			}
		}
	}
}

// Scan all visible elements in the art tree
//   Track maximum bounds of visible artwork per layer
//   Track pattern fills used by visible artwork
//   Track if gradient fills are used by visible artwork per layer
void HtmlDocument::ScanDocument()
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

void HtmlDocument::ScanLayer(Layer& layer)
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
void HtmlDocument::ScanLayerArtwork(AIArtHandle artHandle, unsigned int depth, Layer& layer)
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
				bool added = canvas->documentResources->patterns.Add(symbolPatternHandle, true);

				// If we added a new pattern, scan its artwork
				if (added)
				{
					AIArtHandle patternArtHandle = NULL;
					sAIPattern->GetPatternArt(symbolPatternHandle, &patternArtHandle);

					// Look inside, but don't screw up bounds for our current layer
					Layer symbolLayer;
					ScanLayerArtwork(patternArtHandle, (depth + 1), symbolLayer);

					// Capture features for pattern
					Pattern* pattern = canvas->documentResources->patterns.Find(symbolPatternHandle);
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
						canvas->documentResources->patterns.Add(style.fill.color.c.p.pattern, false);

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
						canvas->documentResources->patterns.Add(style.stroke.color.c.p.pattern, false);

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
	}
	while (artHandle != NULL);
}

// Creates the JavaScript animation file (if it doesn't already exist)
void HtmlDocument::CreateAnimationFile()
{
	// Full path to JavaScript animation support file
	std::string fullPath = resources.folderPath + "Ai2CanvasAnimation.js";

	// Ensure that the file doesn't already exist
	if (!FileExists(fullPath))
	{
		ofstream animFile;

		// Create the file
		animFile.open(fullPath.c_str(), ios::out);

		// Is the file open?
		if (animFile.is_open())
		{
			// Header
			OutputScriptHeader(animFile);

			// Clock functions
			OutputClockFunctions(animFile);

			// Animation functions
			OutputAnimationFunctions(animFile);

			// Easing functions
			OutputTimingFunctions(animFile);
		}

		// Close the file
		animFile.close();
	}
}

void HtmlDocument::OutputScriptHeader(ofstream& file)
{
	file <<     "// Ai2CanvasAnimation.js Version " << PLUGIN_VERSION;
	file << "// Animation support for the Ai->Canvas Export Plug-In" << endl;
	file << "// By Mike Swanson (http://blog.mikeswanson.com/)" << endl;
}

void HtmlDocument::OutputClockFunctions(ofstream& file)
{
	file << "// Create a shared standard clock" << endl;
	file << "var timeProvider = new standardClock();" << endl;
	file << "// All animation clocks" << endl;
	file << "var clocks = new Array();" << endl;
	file << "// Represents an animation clock" << endl;
	file << "function clock(duration, delay, direction, reverses, iterations, timingFunction, range, multiplier, offset) {" << endl;
	file << "// Initialize" << endl;
	file << "this.timeProvider = timeProvider;                 // Time provider" << endl;
	file << "this.duration = duration;                         // Duration (in seconds)" << endl;
	file << "this.delay = delay;                               // Initial delay (in seconds)" << endl;
	file << "this.direction = direction;                       // Direction (-1 = backward, 1 = forward)" << endl;
	file << "this.reverses = reverses;                         // Does this reverse? (true/false)" << endl;
	file << "this.iterations = iterations;                     // Number of iterations (0 = infinite)" << endl;
	file << "this.timingFunction = timingFunction;             // Timing function" << endl;
	file << "this.multiplier = (range * multiplier);           // Value multiplier (after timing function)" << endl;
	file << "this.offset = (range * offset);                   // Value offset (after multiplier)" << endl;
	file << "// Reset the clock" << endl;
	file << "this.reset = function () {" << endl;
	file << "this.startTime = 0;                             // Start time reference" << endl;
	file << "this.stopTime = 0;                              // Stop time reference" << endl;
	file << "this.lastTime = 0;                              // Last time reference" << endl;
	file << "this.baseDirection = this.direction;            // Base direction" << endl;
	file << "this.d = this.baseDirection;                    // Current direction" << endl;
	file << "this.t = (this.baseDirection == 1 ? 0.0 : 1.0); // Current clock time (0.0 - 1.0)" << endl;
	file << "this.i = 0;                                     // Current iteration" << endl;
	file << "this.isRunning = false;                         // Is this running?" << endl;
	file << "this.isFinished = false;                        // Is the entire clock run finished?" << endl;
	file << "this.value = 0.0;                               // Current computed clock value" << endl;
	file << "}" << endl;
	file << "// Reset to initial conditions" << endl;
	file << "this.reset();" << endl;
	file << "// Add events" << endl;
	file << "this.started = new customEvent(\"started\");" << endl;
	file << "this.stopped = new customEvent(\"stopped\");" << endl;
	file << "this.iterated = new customEvent(\"iterated\");" << endl;
	file << "this.finished = new customEvent(\"finished\");" << endl;
	file << "// Start the clock" << endl;
	file << "this.start = function () {" << endl;
	file << "// Only start if the clock isn't running and it hasn't finished" << endl;
	file << "if (!this.isRunning && !this.isFinished) {" << endl;
	file << "// Capture start time" << endl;
	file << "this.startTime = this.timeProvider.ticks() - (this.stopTime - this.startTime);" << endl;
	file << "// Start the animation" << endl;
	file << "this.isRunning = true;" << endl;
	file << "// Started event" << endl;
	file << "this.started.fire(null, { message: this.started.eventName });" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "// Re-start the clock (reset and start)" << endl;
	file << "this.restart = function () {" << endl;
	file << "this.reset();" << endl;
	file << "this.start();" << endl;
	file << "}" << endl;
	file << "// Stop the clock" << endl;
	file << "this.stop = function () {" << endl;
	file << "// Only stop if the clock is running and it hasn't finished" << endl;
	file << "if (this.isRunning && !this.isFinished) {" << endl;
	file << "// Capture stop time" << endl;
	file << "this.stopTime = this.timeProvider.ticks();" << endl;
	file << "// Stop the animation" << endl;
	file << "this.isRunning = false;" << endl;
	file << "// Stopped event" << endl;
	file << "this.stopped.fire(null, { message: this.stopped.eventName });" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "// Toggle the clock" << endl;
	file << "this.toggle = function () {" << endl;
	file << "// Only toggle the clock if it hasn't finished" << endl;
	file << "if (!this.isFinished) {" << endl;
	file << "// Is the clock running?" << endl;
	file << "if (this.isRunning) {" << endl;
	file << "// Stop the clock" << endl;
	file << "this.stop();" << endl;
	file << "}" << endl;
	file << "else {" << endl;
	file << "// Start the clock" << endl;
	file << "this.start();" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "// Rewind the clock" << endl;
	file << "this.rewind = function () {" << endl;
	file << "// Only rewind if the clock is running and it hasn't finished" << endl;
	file << "if (this.isRunning && !this.isFinished) {" << endl;
	file << "// Rewind to the beginning of the current iteration" << endl;
	file << "this.jumpTo(this.i);" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "// Fast-forward the clock" << endl;
	file << "this.fastForward = function () {" << endl;
	file << "// Only fast-forward if the clock is running and it hasn't finished" << endl;
	file << "if (this.isRunning && !this.isFinished) {" << endl;
	file << "// Fast-forward to the beginning of the next iteration" << endl;
	file << "this.jumpTo(this.i + 1);" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "// Reverse the clock" << endl;
	file << "this.reverse = function () {" << endl;
	file << "// Only reverse if the clock is running and it hasn't finished" << endl;
	file << "if (this.isRunning && !this.isFinished) {" << endl;
	file << "// Reverse the clock direction" << endl;
	file << "this.baseDirection = -this.baseDirection;" << endl;
	file << "// Jump to the same position, but in reverse" << endl;
	file << "var position = this.i + (this.d == -1.0 ? this.t : (1.0 - this.t));" << endl;
	file << "this.jumpTo(position);" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "// Jump to iteration" << endl;
	file << "this.jumpTo = function(iteration) {" << endl;
	file << "// Determine iteration time" << endl;
	file << "var now = this.timeProvider.ticks();" << endl;
	file << "var ticksPerSecond = this.timeProvider.ticksPerSecond();" << endl;
	file << "var iterationTime = (this.delay * ticksPerSecond) + " << endl;
	file << "((iteration * this.duration) * ticksPerSecond);" << endl;
	file << "this.startTime = (now - iterationTime);" << endl;
	file << "}" << endl;
	file << "// Update function" << endl;
	file << "this.update = updateClock;" << endl;
	file << "// Set initial value" << endl;
	file << "this.value = (this.timingFunction(this.t) * this.multiplier) + this.offset;" << endl;
	file << "// Add to clocks array" << endl;
	file << "clocks.push(this);" << endl;
	file << "}" << endl;
	file << "// Update clock state" << endl;
	file << "function updateClock() {" << endl;
	file << "// Is clock running?" << endl;
	file << "if (this.isRunning && !this.isFinished) {" << endl;
	file << "// Capture the current time" << endl;
	file << "var now = this.timeProvider.ticks();" << endl;
	file << "// Has the time changed?" << endl;
	file << "if (now != this.lastTime) {" << endl;
	file << "// How many seconds have elapsed since the clock started?" << endl;
	file << "var elapsed = (now - this.startTime) / this.timeProvider.ticksPerSecond();" << endl;
	file << "// How many possible iterations?" << endl;
	file << "var iterations = (elapsed - this.delay) / this.duration;" << endl;
	file << "// Need to wait more?" << endl;
	file << "if (iterations < 0.0) {" << endl;
	file << "// Reset to 0" << endl;
	file << "iterations = 0.0;" << endl;
	file << "}" << endl;
	file << "// Capture current iteration" << endl;
	file << "var currentIteration = Math.floor(iterations);" << endl;
	file << "// Iteration changed?" << endl;
	file << "if (currentIteration != this.i) {" << endl;
	file << "// Iterated event" << endl;
	file << "this.iterated.fire(null, { message: this.iterated.eventName });" << endl;
	file << "}" << endl;
	file << "// How far \"into\" the iteration?" << endl;
	file << "this.t = iterations - currentIteration;" << endl;
	file << "// Is this finite?" << endl;
	file << "if (this.iterations != 0) {" << endl;
	file << "// Reached the limit?" << endl;
	file << "if (currentIteration >= this.iterations) {" << endl;
	file << "// Set to end of final iteration" << endl;
	file << "currentIteration = this.iterations - 1;" << endl;
	file << "this.t = 1.0;" << endl;
	file << "// Stop clock" << endl;
	file << "this.stop();" << endl;
	file << "// This clock has finished" << endl;
	file << "this.isFinished = true;" << endl;
	file << "// Finished event" << endl;
	file << "this.finished.fire(null, { message: this.finished.eventName });" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "// Track current iteration" << endl;
	file << "this.i = currentIteration;" << endl;
	file << "// Does direction ever change?" << endl;
	file << "if (this.reverses) {" << endl;
	file << "// Is this an even iteration? (0 is considered even)" << endl;
	file << "if ((Math.floor(this.i) % 2) == 0) {" << endl;
	file << "// Original direction" << endl;
	file << "this.d = this.baseDirection;" << endl;
	file << "}" << endl;
	file << "else {" << endl;
	file << "// Alternate direction" << endl;
	file << "this.d = -this.baseDirection;" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "else {" << endl;
	file << "// Direction doesn't change" << endl;
	file << "this.d = this.baseDirection;" << endl;
	file << "}" << endl;
	file << "// Moving \"backwards\"?" << endl;
	file << "if (this.d == -1) {" << endl;
	file << "// Adjust \"t\"" << endl;
	file << "this.t = (1.0 - this.t);" << endl;
	file << "}" << endl;
	file << "// Update current computed clock value" << endl;
	file << "this.value = (this.timingFunction(this.t) * this.multiplier) + this.offset;" << endl;
	file << "// Remember last time" << endl;
	file << "this.lastTime = now;" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "// Update all animation clocks" << endl;
	file << "function updateAllClocks() {" << endl;
	file << "// Loop through clocks" << endl;
	file << "var clockCount = clocks.length;" << endl;
	file << "for (var i = 0; i < clockCount; i++) {" << endl;
	file << "// Update clock" << endl;
	file << "clocks[i].update();" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "// Standard clock" << endl;
	file << "function standardClock() {" << endl;
	file << "// Return current tick count" << endl;
	file << "this.ticks = function() {" << endl;
	file << "return new Date().getTime();" << endl;
	file << "}" << endl;
	file << "// Return number of ticks per second" << endl;
	file << "this.ticksPerSecond = function() {" << endl;
	file << "return 1000;" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "// Custom event" << endl;
	file << "function customEvent() {" << endl;
	file << "// Name of the event" << endl;
	file << "this.eventName = arguments[0];" << endl;
	file << "// Subscribers to notify on event fire" << endl;
	file << "this.subscribers = new Array();" << endl;
	file << "// Subscribe a function to the event" << endl;
	file << "this.subscribe = function(fn) {" << endl;
	file << "// Only add if the function doesn't already exist" << endl;
	file << "if (this.subscribers.indexOf(fn) == -1) {" << endl;
	file << "// Add the function" << endl;
	file << "this.subscribers.push(fn);" << endl;
	file << "}" << endl;
	file << "};" << endl;
	file << "// Fire the event" << endl;
	file << "this.fire = function(sender, eventArgs) {" << endl;
	file << "// Any subscribers?" << endl;
	file << "if (this.subscribers.length > 0) {" << endl;
	file << "// Loop through all subscribers" << endl;
	file << "for (var i = 0; i < this.subscribers.length; i++) {" << endl;
	file << "// Notify subscriber" << endl;
	file << "this.subscribers[i](sender, eventArgs);" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "};" << endl;
	file << "};" << endl;
}

void HtmlDocument::OutputAnimationFunctions(ofstream& file)
{
	// Animation path support functions
	file << "// Updates animation path" << endl;
	file << "function updatePath() {" << endl;
	file << "// Reference the animation path clock" << endl;
	file << "var clock = this.pathClock;" << endl;
	file << "// Where is T in the linear animation?" << endl;
	file << "var t = clock.value;" << endl;
	file << "// Has the clock value changed?" << endl;
	file << "if (t != this.lastValue) {" << endl;
	file << "// Limit t" << endl;
	file << "if (t < 0.0 || t > (this.linear.length - 1)) {" << endl;
	file << "t = (t < 0.0) ? 0.0 : (this.linear.length - 1);" << endl;
	file << "}" << endl;
	file << "var tIndex = Math.floor(t);" << endl;
	file << "// Distance between index points" << endl;
	file << "var d = (t - tIndex);" << endl;
	file << "// Get segment indices" << endl;
	file << "var segment1Index = this.linear[tIndex][0];" << endl;
	file << "var segment2Index = segment1Index;" << endl;
	file << "// U values to interpolate between" << endl;
	file << "var u1 = this.linear[tIndex][1];" << endl;
	file << "var u2 = u1;" << endl;
	file << "// Get T values" << endl;
	file << "var t1 = this.linear[tIndex][2];" << endl;
	file << "var t2 = t1;" << endl;
	file << "// If in bounds, grab second segment" << endl;
	file << "if ((tIndex + 1) < (this.linear.length))" << endl;
	file << "{" << endl;
	file << "var segment2Index = this.linear[(tIndex + 1)][0];" << endl;
	file << "var u2 = this.linear[(tIndex + 1)][1];" << endl;
	file << "var t2 = this.linear[(tIndex + 1)][2];" << endl;
	file << "}" << endl;
	file << "// Segment index and U value" << endl;
	file << "var segmentIndex = segment1Index;" << endl;
	file << "var u = 0.0;" << endl;
	file << "// Interpolate" << endl;
	file << "// Same segment?" << endl;
	file << "if (segment1Index == segment2Index)" << endl;
	file << "{" << endl;
	file << "// Interpolate U value" << endl;
	file << "u = (d * (u2 - u1)) + u1;" << endl;
	file << "}" << endl;
	file << "else" << endl;
	file << "{" << endl;
	file << "// Difference in T" << endl;
	file << "var deltaT = t2 - t1;" << endl;
	file << "// Based on distance, how \"far\" are we along T?" << endl;
	file << "var tDistance = d * deltaT;" << endl;
	file << "// How much segment 1 T?" << endl;
	file << "var segment1T = (this.segmentT[segment1Index] - t1);" << endl;
	file << "// Part of the first segment (before the anchor point)?" << endl;
	file << "if ((t1 + tDistance) < this.segmentT[segment1Index])" << endl;
	file << "{" << endl;
	file << "// How far along?" << endl;
	file << "var p = (segment1T == 0 ? 0 : tDistance / segment1T);" << endl;
	file << "// Compute U" << endl;
	file << "u = ((1.0 - u1) * p) + u1;" << endl;
	file << "}" << endl;
	file << "else" << endl;
	file << "{" << endl;
	file << "// Beginning of second segment" << endl;
	file << "segmentIndex = segment2Index;" << endl;
	file << "// How much segment 2 T?" << endl;
	file << "var segment2T = (t2 - this.segmentT[segment1Index]);" << endl;
	file << "// How much T remains in this segment?" << endl;
	file << "var tRemaining = tDistance - segment1T;" << endl;
	file << "// How far along?" << endl;
	file << "var p = (segment2T == 0 ? 0 : tRemaining / segment2T);" << endl;
	file << "// Compute U" << endl;
	file << "u = p * u2;" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "// Calculate bezier curve position" << endl;
	file << "this.x = bezier(u," << endl;
	file << "this.points[segmentIndex][0][0]," << endl;
	file << "this.points[segmentIndex][1][0]," << endl;
	file << "this.points[segmentIndex][2][0]," << endl;
	file << "this.points[segmentIndex][3][0]);" << endl;
	file << "this.y = bezier(u," << endl;
	file << "this.points[segmentIndex][0][1]," << endl;
	file << "this.points[segmentIndex][1][1]," << endl;
	file << "this.points[segmentIndex][2][1]," << endl;
	file << "this.points[segmentIndex][3][1]);" << endl;
	file << "// Determine follow orientation" << endl;
	file << "var qx = 0.0;" << endl;
	file << "var qy = 0.0;" << endl;
	file << "// At a 0.0 or 1.0 boundary?" << endl;
	file << "if (u == 0.0) {" << endl;
	file << "// Use control point" << endl;
	file << "qx = this.points[segmentIndex][1][0];" << endl;
	file << "qy = this.points[segmentIndex][1][1];" << endl;
	file << "this.orientation = followOrientation(this.x, this.y, qx, qy, clock.d);" << endl;
	file << "}" << endl;
	file << "else if (u == 1.0) {" << endl;
	file << "// Use control point" << endl;
	file << "qx = this.points[segmentIndex][1][0];" << endl;
	file << "qy = this.points[segmentIndex][1][1];" << endl;
	file << "this.orientation = followOrientation(qx, qy, this.x, this.y, clock.d);" << endl;
	file << "}" << endl;
	file << "else {" << endl;
	file << "// Calculate quadratic curve position" << endl;
	file << "qx = quadratic(u," << endl;
	file << "this.points[segmentIndex][0][0]," << endl;
	file << "this.points[segmentIndex][1][0]," << endl;
	file << "this.points[segmentIndex][2][0]);" << endl;
	file << "qy = quadratic(u," << endl;
	file << "this.points[segmentIndex][0][1]," << endl;
	file << "this.points[segmentIndex][1][1]," << endl;
	file << "this.points[segmentIndex][2][1]);" << endl;
	file << "this.orientation = followOrientation(qx, qy, this.x, this.y, clock.d);" << endl;
	file << "}" << endl;
	file << "// Remember this clock value" << endl;
	file << "this.lastValue = t;" << endl;
	file << "}" << endl;
	file << "// Update clock" << endl;
	file << "clock.update();" << endl;
	file << "}" << endl;
	file << "// Returns follow orientation" << endl;
	file << "function followOrientation(x1, y1, x2, y2, direction) {" << endl;
	file << "// Forward?" << endl;
	file << "if (direction == 1) {" << endl;
	file << "return slope(x1, y1, x2, y2);" << endl;
	file << "}" << endl;
	file << "else {" << endl;
	file << "return slope(x2, y2, x1, y1);" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "// Returns a position along a cubic Bezier curve" << endl;
	file << "function bezier(u, p0, p1, p2, p3) {" << endl;
	file << "return Math.pow(u, 3) * (p3 + 3 * (p1 - p2) - p0)" << endl;
	file << "+ 3 * Math.pow(u, 2) * (p0 - 2 * p1 + p2)" << endl;
	file << "+ 3 * u * (p1 - p0) + p0;" << endl;
	file << "}" << endl;
	file << "// Returns a position along a quadratic curve" << endl;
	file << "function quadratic(u, p0, p1, p2) {" << endl;
	file << "u = Math.max(Math.min(1.0, u), 0.0);" << endl;
	file << "return Math.pow((1.0 - u), 2) * p0 +" << endl;
	file << "2 * u * (1.0 - u) * p1 +" << endl;
	file << "u * u * p2;" << endl;
	file << "}" << endl;
	file << "// Returns the slope between two points" << endl;
	file << "function slope(x1, y1, x2, y2) {" << endl;
	file << "var dx = (x2 - x1);" << endl;
	file << "var dy = (y2 - y1);" << endl;
	file << "return Math.atan2(dy, dx);" << endl;
	file << "}" << endl;
}

void HtmlDocument::OutputTimingFunctions(ofstream& file)
{
	// Timing functions
	file << "// Penner timing functions" << endl;
	file << "// Based on Robert Penner's easing equations: http://www.robertpenner.com/easing/" << endl;
	file << "function linear(t) {" << endl;
	file << "return t;" << endl;
	file << "}" << endl;
	file << "function sineEaseIn(t) {" << endl;
	file << "return -Math.cos(t * (Math.PI/2)) + 1;" << endl;
	file << "}" << endl;
	file << "function sineEaseOut(t) {" << endl;
	file << "return Math.sin(t * (Math.PI/2));" << endl;
	file << "}" << endl;
	file << "function sineEaseInOut(t) {" << endl;
	file << "return -0.5 * (Math.cos(Math.PI * t) - 1);" << endl;
	file << "}" << endl;
	file << "function quintEaseIn(t) {" << endl;
	file << "return t * t * t * t * t;" << endl;
	file << "}" << endl;
	file << "function quintEaseOut(t) {" << endl;
	file << "t--;" << endl;
	file << "return t * t * t * t * t + 1;" << endl;
	file << "}" << endl;
	file << "function quintEaseInOut(t) {" << endl;
	file << "t /= 0.5;" << endl;
	file << "if (t < 1) { return 0.5 * t * t * t * t * t; }" << endl;
	file << "t -= 2;" << endl;
	file << "return 0.5 * (t * t * t * t * t + 2);" << endl;
	file << "}" << endl;
	file << "function quartEaseIn(t) {" << endl;
	file << "return t * t * t * t;" << endl;
	file << "}" << endl;
	file << "function quartEaseOut(t) {" << endl;
	file << "t--;" << endl;
	file << "return -(t * t * t * t - 1);" << endl;
	file << "}" << endl;
	file << "function quartEaseInOut(t) {" << endl;
	file << "t /= 0.5;" << endl;
	file << "if (t < 1) { return 0.5 * t * t * t * t; }" << endl;
	file << "t -= 2;" << endl;
	file << "return -0.5 * (t * t * t * t - 2);" << endl;
	file << "}" << endl;
	file << "function circEaseIn(t) {" << endl;
	file << "return -(Math.sqrt(1 - (t * t)) - 1);" << endl;
	file << "}" << endl;
	file << "function circEaseOut(t) {" << endl;
	file << "t--;" << endl;
	file << "return Math.sqrt(1 - (t * t));" << endl;
	file << "}" << endl;
	file << "function circEaseInOut(t) {" << endl;
	file << "t /= 0.5;" << endl;
	file << "if (t < 1) { return -0.5 * (Math.sqrt(1 - t * t) - 1); }" << endl;
	file << "t-= 2;" << endl;
	file << "return 0.5 * (Math.sqrt(1 - t * t) + 1);" << endl;
	file << "}" << endl;
	file << "function quadEaseIn(t) {" << endl;
	file << "return t * t;" << endl;
	file << "}" << endl;
	file << "function quadEaseOut(t) {" << endl;
	file << "return -1.0 * t * (t - 2.0);" << endl;
	file << "}" << endl;
	file << "function quadEaseInOut(t) {" << endl;
	file << "t /= 0.5;" << endl;
	file << "if (t < 1.0) {" << endl;
	file << "return 0.5 * t * t;" << endl;
	file << "}" << endl;
	file << "t--;" << endl;
	file << "return -0.5 * (t * (t - 2.0) - 1);" << endl;
	file << "}" << endl;
	file << "function cubicEaseIn(t) {" << endl;
	file << "return t * t * t;" << endl;
	file << "}" << endl;
	file << "function cubicEaseOut(t) {" << endl;
	file << "t--;" << endl;
	file << "return t * t * t + 1;" << endl;
	file << "}" << endl;
	file << "function cubicEaseInOut(t) {" << endl;
	file << "t /= 0.5;" << endl;
	file << "if (t < 1) { return 0.5 * t * t * t; }" << endl;
	file << "t -= 2;" << endl;
	file << "return 0.5 * (t * t * t + 2);" << endl;
	file << "}" << endl;
	file << "function bounceEaseOut(t) {" << endl;
	file << "if (t < (1.0 / 2.75)) {" << endl;
	file << "return (7.5625 * t * t);" << endl;
	file << "} else if (t < (2 / 2.75)) {" << endl;
	file << "t -= (1.5 / 2.75);" << endl;
	file << "return (7.5625 * t * t + 0.75);" << endl;
	file << "} else if (t < (2.5 / 2.75)) {" << endl;
	file << "t -= (2.25 / 2.75);" << endl;
	file << "return (7.5625 * t * t + 0.9375);" << endl;
	file << "} else {" << endl;
	file << "t -= (2.625 / 2.75);" << endl;
	file << "return (7.5625 * t * t + 0.984375);" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "function bounceEaseIn(t) {" << endl;
	file << "return 1.0 - bounceEaseOut(1.0 - t);" << endl;
	file << "}" << endl;
	file << "function bounceEaseInOut(t) {" << endl;
	file << "if (t < 0.5) {" << endl;
	file << "return bounceEaseIn(t * 2.0) * 0.5;" << endl;
	file << "} else {" << endl;
	file << "return bounceEaseOut(t * 2.0 - 1.0) * 0.5 + 0.5;" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "function expoEaseIn(t) {" << endl;
	file << "return (t == 0.0) ? 0.0 : Math.pow(2.0, 10.0 * (t - 1));" << endl;
	file << "}" << endl;
	file << "function expoEaseOut(t) {" << endl;
	file << "return (t == 1.0) ? 1.0 : -Math.pow(2.0, -10.0 * t) + 1.0;" << endl;
	file << "}" << endl;
	file << "function expoEaseInOut(t) {" << endl;
	file << "if (t == 0) {" << endl;
	file << "return 0.0;" << endl;
	file << "} else if (t == 1.0) {" << endl;
	file << "return 1.0;" << endl;
	file << "} else if ((t / 0.5) < 1.0) {" << endl;
	file << "t /= 0.5;" << endl;
	file << "return 0.5 * Math.pow(2.0, 10.0 * (t - 1));" << endl;
	file << "} else {" << endl;
	file << "t /= 0.5;" << endl;
	file << "return 0.5 * (-Math.pow(2.0, -10.0 * (t - 1)) + 2);" << endl;
	file << "}" << endl;
	file << "}" << endl;
	file << "// Other timing functions" << endl;
	file << "function zeroStep(t) {" << endl;
	file << "return (t <= 0.0 ? 0.0 : 1.0);" << endl;
	file << "}" << endl;
	file << "function halfStep(t) {" << endl;
	file << "return (t < 0.5 ? 0.0 : 1.0);" << endl;
	file << "}" << endl;
	file << "function oneStep(t) {" << endl;
	file << "return (t >= 1.0 ? 1.0 : 0.0);" << endl;
	file << "}" << endl;
	file << "function random(t) {" << endl;
	file << "return Math.random();" << endl;
	file << "}" << endl;
	file << "function randomLimit(t) {" << endl;
	file << "return Math.random() * t;" << endl;
	file << "}" << endl;
	file << "function clockTick(t) {" << endl;
	file << "var steps = 60.0;" << endl;
	file << "return Math.floor(t * steps) / steps;" << endl;
	file << "}" << endl;
}

void HtmlDocument::RenderSymbolFunctions()
{
	// Do we have symbol functions to render?
	if (canvas->documentResources->patterns.HasSymbols())
	{
		// Loop through symbols
		for (unsigned int i = 0; i < canvas->documentResources->patterns.Patterns().size(); i++)
		{
			// Is this a symbol?
			if (canvas->documentResources->patterns.Patterns()[i]->isSymbol)
			{
				// Pointer to pattern (for convenience)
				Pattern* pattern = canvas->documentResources->patterns.Patterns()[i];

				// Begin symbol function block
				outFile << "function " << pattern->name << "(ctx) {" << endl;

				// Need a blank line?
				if (pattern->hasAlpha || pattern->hasGradients || pattern->hasPatterns)
				{
					outFile  << endl;
				}

				// Does this draw function have alpha changes?
				if (pattern->hasAlpha)
				{
					// Grab the alpha value (so we can use it to compute new globalAlpha values during this draw function)
					outFile  << Indent(0) << "var alpha = ctx.globalAlpha;" << endl;
				}

				// Will we be encountering gradients?
				if (pattern->hasGradients)
				{
					outFile  << Indent(0) << "var gradient;" << endl;
				}

				// Will we be encountering patterns?
				// TODO: Is this even possible?
				if (pattern->hasPatterns)
				{
					outFile  << Indent(0) << "var pattern;" << endl;
				}

				// Get a handle to the pattern art
				AIArtHandle patternArtHandle = nil;
				sAIPattern->GetPatternArt(pattern->patternHandle, &patternArtHandle);

				// While we're here, get the size of this canvas
				AIRealRect bounds;
				sAIArt->GetArtBounds(patternArtHandle, &bounds);
				if (debug)
				{
					outFile  << Indent(0) << "// Symbol art bounds = " <<
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

				// End function block
				outFile << "}" << endl;
			}
		}
	}
}

void HtmlDocument::RenderPatternFunction()
{
	// Do we have pattern functions to render?
	if (canvas->documentResources->patterns.HasPatterns())
	{
		// Begin pattern function block
		outFile << "function drawPatterns() {" << endl;

		// Loop through patterns
		for (unsigned int i = 0; i < canvas->documentResources->patterns.Patterns().size(); i++)
		{
			// Is this a pattern?
			if (!canvas->documentResources->patterns.Patterns()[i]->isSymbol)
			{
				// Allocate space for pattern name
				ai::UnicodeString patternName;

				// Pointer to pattern (for convenience)
				Pattern* pattern = canvas->documentResources->patterns.Patterns()[i];

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
				outFile  << Indent(1) << "var " << canvas->id << " = document.getElementById(\"" << canvas->id << "\");" << endl;
				outFile  << Indent(1) << "var " << canvas->contextName << " = " << canvas->id << ".getContext(\"2d\");" << endl;

				// Get a handle to the pattern art
				AIArtHandle patternArtHandle = nil;
				sAIPattern->GetPatternArt(pattern->patternHandle, &patternArtHandle);

				// While we're here, get the size of this canvas
				AIRealRect bounds;
				sAIArt->GetArtBounds(patternArtHandle, &bounds);
				if (debug)
				{
					outFile  << Indent(0) << "// Symbol art bounds = " <<
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
					sAIRealMath->AIRealMatrixConcatScale(&canvas->currentState->internalTransform, 1,  - 1);
					sAIRealMath->AIRealMatrixConcatTranslate(&canvas->currentState->internalTransform,  - 1 * bounds.left, bounds.top);
					sAIRealMath->AIRealMatrixConcatScale(&canvas->currentState->internalTransform, 1,  - 1);
					sAIRealMath->AIRealMatrixConcatTranslate(&canvas->currentState->internalTransform,  0, canvas->height);
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

		// End function block
		outFile << "}" << endl;
	}
}

void HtmlDocument::DebugInfo()
{
	outFile << "<p>This document has been exported in debug mode.</p>" << endl;

	if (hasAnimation)
	{
		outFile << "<p>To scrub animations, click a Y location to set the time window, then move left/right to scrub.</p>" << endl;
	}

	resources.images.DebugInfo();

	functions.DebugInfo();
}

void HtmlDocument::DebugClockJS()
{
	outFile << "// Debug clock" << endl;
	outFile << "function debugClock() {" << endl;
	outFile << "// Mouse state" << endl;
	outFile << "this.mouseX = 0;" << endl;
	outFile << "this.mouseY = 0;" << endl;
	outFile << "this.resetMouse = true;" << endl;
	outFile << "// Y location on mouseDown" << endl;
	outFile << "this.y = 0.0;" << endl;
	outFile << "// Time range" << endl;
	outFile << "this.timeRange = 0.0;" << endl;
	outFile << "// Return current tick count" << endl;
	outFile << "this.ticks = function() {" << endl;
	outFile << "// Reset Y?    " << endl;
	outFile << "if (this.resetMouse) {" << endl;
	outFile << "// Capture Y" << endl;
	outFile << "this.y = this.mouseY;" << endl;
	outFile << "// Update time range" << endl;
	outFile << "this.timeRange = (this.y / " << canvas->id << ".height) * 120;" << endl;
	outFile << "this.resetMouse = false;" << endl;
	outFile << "}" << endl;
	outFile << "return ((this.mouseX / " << canvas->id << ".width) * this.timeRange * 1000);" << endl;
	outFile << "}" << endl;
	outFile << "// Return number of ticks per second" << endl;
	outFile << "this.ticksPerSecond = function() {" << endl;
	outFile << "return 1000;" << endl;
	outFile << "}" << endl;
	outFile << "}" << endl;

	outFile << "function setDebugClock() {" << endl;
	outFile << "debug.resetMouse = true;" << endl;
    outFile << "}" << endl;

	outFile << "function getMouseLocation(e) {" << endl;
    outFile << "debug.mouseX = e.clientX + document.body.scrollLeft +" << endl;
	outFile << "document.documentElement.scrollLeft - canvas.offsetLeft;" << endl;
    outFile << "debug.mouseY = e.clientY + document.body.scrollTop +" << endl;
	outFile << "document.documentElement.scrollTop - canvas.offsetTop;" << endl;
    outFile << "}" << endl;
}

void HtmlDocument::DebugAnimationPathJS()
{
	outFile << "function plotAnchorPoints(ctx) {" << endl;

	outFile << "ctx.save();" << endl;
    outFile << "ctx.fillStyle = \"rgb(255, 0, 0)\";" << endl;
    outFile << "var animation;" << endl;
	outFile << "var animationCount = animations.length;" << endl;
	outFile << "for (var a = 0; a < animationCount; a++) {" << endl;
	outFile << "animation = animations[a];" << endl;
	outFile << "var pointCount = animation.points.length;" << endl;
	outFile << "for (var i = 0; i < pointCount; i++) {" << endl;
	outFile << "ctx.fillRect(animation.points[i][0][0] - 2, animation.points[i][0][1] - 2, 5, 5);" << endl;
    outFile << "}" << endl;
    outFile << "}" << endl;
	outFile << "// Final anchor point" << endl;
	outFile << "ctx.fillRect(animation.points[(animation.points.length - 1)][3][0] - 2," << endl;
	outFile << "animation.points[(animation.points.length - 1)][3][1] - 2, 5, 5);" << endl;
	outFile << "ctx.restore();" << endl;
	outFile << "}" << endl;

	outFile << "function plotLinearPoints(ctx) {" << endl;
	outFile << "ctx.save();" << endl;
    outFile << "ctx.fillStyle = \"rgb(0, 0, 255)\";" << endl;
	outFile << "var animationCount = animations.length;" << endl;
	outFile << "for (var a = 0; a < animationCount; a++) {" << endl;
	outFile << "var animation = animations[a];" << endl;
	outFile << "var linearCount = animation.linear.length;" << endl;
	outFile << "for (var i = 0; i < linearCount; i++) {" << endl;
	outFile << "var segmentIndex = animation.linear[i][0];" << endl;
    outFile << "var u = animation.linear[i][1];" << endl;
	outFile << "var x = bezier(u," << endl;
	outFile << "animation.points[segmentIndex][0][0]," << endl;
    outFile << "animation.points[segmentIndex][1][0]," << endl;
	outFile << "animation.points[segmentIndex][2][0]," << endl;
	outFile << "animation.points[segmentIndex][3][0]);" << endl;
	outFile << "var y = bezier(u," << endl;
	outFile << "animation.points[segmentIndex][0][1]," << endl;
	outFile << "animation.points[segmentIndex][1][1]," << endl;
	outFile << "animation.points[segmentIndex][2][1]," << endl;
	outFile << "animation.points[segmentIndex][3][1]);" << endl;
	outFile << "ctx.fillRect(x - 1, y - 1, 3, 3);" << endl;
    outFile << "}" << endl;
    outFile << "}" << endl;
	outFile << "ctx.restore();" << endl;
	outFile << "}" << endl;
}