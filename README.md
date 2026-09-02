# Retained Mode UI Engine

This is a very basic UI Engine i have been working on as a personal project. The engine contains a renderer, a batcher, a layout hierarchy manager and an object storage / lifetime manager.
The core loop of the engine works like this:

Every frame, the view port resolution is passed to the engine and StepFrame() is called. StepFrame takes the aforementioned resolution variable as an input. 
Based on simply hierarchy dirtiness checks, the function determines whether or not to rebuild the frame data, upload new frame data or re-evaluate the layout and absolute positioning of elements and the constructed batches, etc.

## End-Goal:

The end goal is for this engine to be a simple-to-use graphics engine that makes it easy to create apps with C++. It is meant to be a high performance alternative to web-based solutions like electron.
Elements will be easy to declare and edit on the fly, editing element properties will feel very similar to vanilla JavaScript's ``element.style.property = value`` syntax.

## Future Plans

The engine is in a stable but very primitive state as of now. I plan on adding support for:
- Shader flags to allow for corner rounding, borders, anti-aliasing and more using SDFs.
- Instanced rendering
- Texture support
I am currently working on these changes and additions. The current UI Engine can be used to make very simple graphical apps but does not have capabilities for input handling, displaying text and alot of other expected modern features. Despite this, it is a UI Engine and it does work.
main.cpp is a demo file.

### Requires glad, KHRplatform and glfw to function. 
Please put the header files for these libraries in separate include directory sub-folders like ``/include/KHR/khrplatform.h`` and ``.c`` or ``.c++`` files in the ``/src/`` directory to run this project.

### Using the Engine

Replace the code inside ``main.cpp`` with your own application code to be able to use the engine. The engine is easily extensible, each newly declared graphical element just needs to inherit from the ``UIElement`` primitive. You can define the behavior for your new ``UIElement`` based object/element by overriding the ``UpdateLayout`` virtual function. To ensure some stability, an ``UpdateLayout`` function override should ideally only change the size and positions of the current element that the function belongs to or its children elements. Changing parents or siblings inside an element's ``UpdateLayout`` function may break things. There might be a direct assertion included in later versions of this engine to outright prevent this with a simple check.
