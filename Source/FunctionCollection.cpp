// FunctionCollection.cpp
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
#include "FunctionCollection.h"

using namespace CanvasExport;

FunctionCollection::FunctionCollection()
{
	// Initialize FunctionCollection
	this->hasDrawFunctions = false;
}

FunctionCollection::~FunctionCollection()
{
	// Clear functions
	for (unsigned int i = 0; i < functions.size(); i++)
	{
		// Remove instance
		delete functions[i];
	}
}

void FunctionCollection::RenderDrawFunctionCalls(const AIRealRect& documentBounds)
{
	// Loop through all draw functions (they're already in order)
	for (unsigned int i = 0; i < functions.size(); i++)
	{
		// Draw function?
		if (functions[i]->type == Function::kDrawFunction)
		{
			// Render draw function call
			((DrawFunction*)functions[i])->RenderDrawFunctionCall(documentBounds);
		}
	}
}

void FunctionCollection::RenderDrawFunctions(const AIRealRect& documentBounds)
{
	// Loop through all draw functions
	for (unsigned int i = 0; i < functions.size(); i++)
	{
		// Draw function?
		if (functions[i]->type == Function::kDrawFunction)
		{
			// Render the function
			((DrawFunction*)functions[i])->RenderDrawFunction(documentBounds);
		}
	}
}

Function* FunctionCollection::Find(const std::string& name, const Function::FunctionType& functionType)
{
	Function* function = NULL;

	// Any functions to search?
	size_t i = functions.size();
	if (i > 0)
	{
		// Loop through functions
		do
		{
			i--;

			// Correct type?
			if ((functions[i]->type == functionType) ||
				(functions[i]->type == Function::kAnyFunction))
			{
				// Do the names match?
				if (functions[i]->name == name)
				{
					// Found a match
					function = functions[i];
					break;
				}
			}
		} while (i > 0);
	}

	// Return result
	return function;
}

// Returns a unique function name (which could be identical to what was passed in, if it's unique)
std::string FunctionCollection::CreateUniqueName(const std::string& name)
{
	// Since we may change the name, copy it first
	std::string usedName = name;

	// Does a function with this name already exist?
	Function* function = Find(usedName, Function::kAnyFunction);

	// Did we find anything?
	if (function)
	{
		// Find a unique name
		std::ostringstream uniqueName;
		int unique = 0;
		do
		{
			// Increment to make unique name
			unique++;

			// Generate a unique name
			uniqueName.str("");
			uniqueName << usedName << unique;
		} while (Find(uniqueName.str(), Function::kAnyFunction));

		// Assign unique name
		usedName = uniqueName.str();
	}

	// Return the unique name
	return usedName;
}

// Adds a new animation function
DrawFunction* FunctionCollection::AddDrawFunction(const std::string& name)
{
	// Since we may change the name, copy it first
	std::string uniqueName = name;

	// Does a function with this name already exist?
	bool isLast = false;
	DrawFunction* drawFunction = FindDrawFunction(uniqueName, isLast);

	// Did we find the function?
	if (drawFunction)
	{
		// We found the function. Is it the most recent drawing function we've added (don't consider animation layers)?
		// Since drawing order is bottoms-up, we can't "go back to" a prior function and add layers to it...it must be unique at that point
		if (!isLast)
		{
			// It's not the most recent function, so we have to make it unique

			// Need a new function
			drawFunction = NULL;

			// Generate unique name
			uniqueName = CreateUniqueName(uniqueName);
		}
	}
	else
	{
		// Matched another function type (perhaps animation), so we need a unique name
		uniqueName = CreateUniqueName(uniqueName);
	}

	// Do we have a function yet?
	if (!drawFunction)
	{
		// Create a new function
		drawFunction = new DrawFunction();

		// Assign names
		drawFunction->requestedName = name;
		drawFunction->name = uniqueName;

		// Add function
		functions.push_back(drawFunction);

		// Note that we have at least one draw function in the collection
		hasDrawFunctions = true;
	}

	// Return the function
	return drawFunction;
}

// Find a draw function
// TODO: Any way we can clean this up? Seems a bit convoluted.
DrawFunction* FunctionCollection::FindDrawFunction(const std::string& name, bool& isLast)
{
	DrawFunction* drawFunction = NULL;

	// By default
	isLast = false;

	// Track if we've seen a draw function yet
	bool passedDrawFunction = false;

	// Any functions to search?
	size_t i = functions.size();
	if (i > 0)
	{
		// Loop through functions
		do
		{
			i--;

			if (functions[i]->type == Function::kDrawFunction)
			{
				// Cast to DrawFunction
				DrawFunction* searchDrawFunction = (DrawFunction*)functions[i];

				// Do the names match?
				if ((searchDrawFunction->name == name) ||
					(searchDrawFunction->requestedName == name))
				{
					// Found a match
					drawFunction = searchDrawFunction;
					break;
				}

				// Can't be the last, if we've made it here
				passedDrawFunction = true;
			}
		} while (i > 0);

		// Did we find something AND it's the last draw function?
		if (drawFunction && (!passedDrawFunction))
		{
			isLast = true;
		}
	}

	// Return result
	return drawFunction;
}

void FunctionCollection::DebugInfo()
{
	// Gather info
	unsigned int drawFunctionCount = 0;

	// Loop through functions
	for (unsigned int i = 0; i < functions.size(); i++)
	{
		switch (functions[i]->type)
		{
			case Function::kDrawFunction:
			{
				++drawFunctionCount;
				break;
			}
            case Function::kAnyFunction:
            {
                break;
            }
		}
	}
	
	// Draw function debug info
	outFile << "<p>Draw functions: " << drawFunctionCount << "</p>" << endl;

	// Anything to list?
	if (drawFunctionCount > 0)
	{
		// Start unordered list
		outFile << "<ul>" << endl;

		// Loop through each draw function
		for (unsigned int i = 0; i < functions.size(); i++)
		{
			if (functions[i]->type == Function::kDrawFunction)
			{
				// For convenience
				DrawFunction* drawFunction = (DrawFunction*)functions[i];

				outFile << "<li>name: " << drawFunction->name << ", layers: " << drawFunction->layers.size() << "</li>" << endl;
			}
		}

		// End unordered list
		outFile << "</ul>" << endl;
	}
}