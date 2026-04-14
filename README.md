# SPATIUM

A real-time 3D Gaussian Splatting viewer built from scratch in C++ and CUDA.

---

## Overview

SPATIUM implements the full forward rasterization pipeline for 3D Gaussian Splatting, from screen-space projection to tile-based alpha blending, entirely in hand-written CUDA kernels. The rendered output is displayed through an interactive OpenGL + ImGui viewer via CUDA-OpenGL interop, with no dependency on existing rasterizer implementations.

The goal is to demonstrate end-to-end understanding of the 3DGS rendering pipeline — from the math in the original paper down to the GPU implementation — and to showcase the engineering required to build a cross-CUDA/OpenGL system in modern C++.
