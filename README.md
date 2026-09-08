# Retained Mode UI Engine

This is a basic UI engine I have been working on as a personal project. The engine contains a renderer, a batcher, a layout hierarchy manager, and an object storage / lifetime manager.

**NOTE:** *`main.cpp` is provided as a basic layout demonstration scene. It is not part of the UI engine. The main engine implementation lives inside ``src/UI/`` and ``include/UI/``.

### Core Loop
Every frame, the viewport resolution is passed to the engine and `StepFrame()` is called. Based on simple hierarchy dirtiness checks, this function determines whether or not to re-evaluate the layout, compute absolute positioning of elements, rebuild the frame data, upload new instance buffers to the GPU, or update the constructed draw commands.

## End-Goal
The end goal is for this engine to be a simple-to-use graphics engine that makes it easy to create desktop applications with C++. It is meant to be a high-performance alternative to web-based solutions like Electron. Elements will be easy to declare and edit on the fly; editing element properties will feel very similar to vanilla JavaScript's `element.style.property = value` syntax.

## Future Plans
The engine is currently in a stable but primitive state. I plan on adding support for:
- Text rendering using pre-existing engine features (font glyph atlases).
- Texture support.

I am currently working on these additions (check out the other development branches for more information). The current UI engine can be used to make simple graphical applications, but it does not yet handle input events, mouse interaction, or display text. Despite these limitations, it functions fully as a foundational UI engine. 

**NOTE:** *`main.cpp` is provided as a basic layout demonstration scene.*

### Requirements
This project requires **glad**, **KHRplatform**, and **glfw** to function. Please place the header files for these libraries in separate include directory sub-folders (such as `/include/KHR/khrplatform.h`) and your `.c` or `.cpp` source files in the `/src/` directory to run this project.

### Using the Engine
To build your own application, replace the demonstration code inside `main.cpp` with your own layout configuration. 

The engine layout system is easily extensible. Custom containers or widgets (like a hypothetical `FlexBoxContainer` or `GridContainer`) inherit from the `UIElement` primitive. You define custom sizing and arrangement behaviors by overriding the virtual `UpdateLayout` function.

To ensure stability across the tree, an `UpdateLayout` override should ideally only change the size and positions of its own element or its direct children. Changing parent or sibling boundaries inside this calculation loop will break positioning; a strict engine assertion may be added in a later version to enforce this rule.

**NOTE:** *Visual primitives (such as padding, corner radii, element color, border widths, and border colors) are built into the engine directly. Every element supports them natively. They should be manipulated using the unified state setters: `EditElementBorder(id, params)`, `EditElementColor(id, params)`, `EditElementCornerRadius(id, params)`, etc.*

### Under the Hood: Hybrid OOP / DOD Architecture
This engine uses a hybrid of Object-Oriented Design (OOP) and Data-Oriented Design (DOD) to maintain hardware rendering performance while preserving the architectural flexibility that comes with object-oriented programming.

* **The Object Layer:** Whenever a new element is created, its pointer lifetime is managed by `UIScene` inside a `std::vector<std::unique_ptr<UIElement>>` list. The `LayoutManager` and specialized structural containers leverage OOP via the virtual `UpdateLayout` function, allowing developers to implement highly customized positioning behaviors.
* **The Data Layer:** During the frame update, the element tree is traversed from the root node down through its children recursively. Element metrics are extracted and compiled into flat, cache-friendly data tables (`dataTables.geometry` and `dataTables.style`). These flat structures are continuously re-used and updated unless an element is added or removed, eliminating pointer-chasing during frame preparation (pointer chasing only happens during hierarchy updates and layout changes).
* **The Traversal Data:** The Hierarchy Manager is responsible for rebuilding, storing, and passing around this linear traversal data.
* **The Render Batcher:** The `RenderBatcher` processes these data arrays and prepares linear data streams for the graphics hardware. Currently, with modern 2D instanced rendering active, it generates draw commands split by clipping paths. Some aspects of this class are artifacts from when the engine still computed individual vertices and used an Element Buffer Object (EBO), but it remains vital for managing scissor intersection states.
* **The Renderer:** The `Renderer` uses modern instanced geometry loops (`glDrawArraysInstancedBaseInstance`) to draw elements with a 32-byte cache-aligned stride, rendering quads purely from `gl_VertexID` calculations. It is the only class in the entire engine pipeline that is aware of OpenGL; no other system executes graphics API commands.
