// JavascriptDocument.h
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

#ifndef JAVASCRIPT_DOCUMENT_H
#define JAVASCRIPT_DOCUMENT_H

#include "IllustratorSDK.h"
#include "Ai2CanvasSuites.h"
#include "Canvas.h"
#include "CanvasCollection.h"
#include "Layer.h"
#include "DocumentResources.h"
#include "FunctionCollection.h"

AIBoolean			ProgressProc(long current, long total);

namespace CanvasExport
{
	// Globals
	extern std::ostream& outFile;
	
	extern bool debug;

	/// Represents a document
	class TypescriptDocument
	{
	private:

		CanvasCollection	canvases;
		FunctionCollection	functions;

		void				SetDocumentBounds();
		void				ParseFolderPath(const std::string& pathName);
		void				RenderDocument();
		void				ScanDocument();
		void				ScanLayer(Layer& layer);
		void				ScanLayerArtwork(AIArtHandle artHandle, unsigned int depth, Layer& layer);
		void				ParseLayers();
		void				ParseLayerName(const Layer& layer, std::string& name, std::string& options);
		void				SetFunctionOptions(const std::vector<std::string>& options, Function& function);
		void				RenderSymbolFunctions();
		void				RenderPatternFunction();
		void				DebugInfo();

	public:

		TypescriptDocument(const std::string& pathName);
		~TypescriptDocument();

		DocumentResources	resources;						// Document resources

		std::vector<Layer*>	layers;							// Layers
		Canvas*				mainCanvas;							// Main document canvas
		std::string			fileName;						// Output file name
		AIRealRect			documentBounds;					// Document bounds (for all visible layers that will be exported)

		void				Render();
	
	};

}

#endif