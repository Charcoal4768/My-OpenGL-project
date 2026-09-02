### Retained Mode UI Engine

This is a very basic UI Engine i have been working on as a personal project. The engine contains a renderer, a batcher, a layout hirearchy manager and an object storage / lifetime manager.
The core loop of the engine works like this:

Every frame, the view port resolution is passed to the engine and StepFrame() is called. StepFrame takes the aforementioned resolution variable as an input. 
Based on simply hierarchy dirtiness checks, the function determines whether or not to rebuild the frame data, upload new frame data, re-evaluate the layout and absolute positioning of elements and the constructed batches, etc.

## Future Plans

The engine is in a stable but very primitive state as of now. I plan on adding support for:
- Shader flags to allow for corner rounding, borders, anti-aliasing and more using SDFs.
- Instanced rendering
- Texture support
I am currently working on these changes and additions. The current UI Engine can be used to make very simple graphical apps but does not have capabilities for input handling, displaying text and alot of other expected modern features. Despite this, it is a UI Engine and it does work.
main.cpp is a demo file.
