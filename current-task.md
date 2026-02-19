# Implement volume rendering

We implement direct volume rendering, with some extensions to make visual fidelity better.

# Plan

# Step 1:

Set up a basic volume raymarcher that can render a 3D texture using raymarching in a fragment shader.

- Create a 3D texture representing the volume data (e.g., a simple density field or a pre-defined dataset).
- Implement a compute shader that performs raymarching through the volume, accumulating color and opacity based on the
  sampled values.
- Set up a compute shader that performs the raymarching

# Step 2:

Extend the raymarcher to support basic lighting and transfer functions.

- Implement a simple lighting model (e.g., Phong or Blinn-Phong) to enhance the visual appearance of the volume.
- Introduce a transfer function that maps scalar values in the volume to color and opacity, allowing for better
  visualization of different structures within the volume.
- Add user controls to adjust the transfer function parameters in real-time.

# Step 3:

Temporal accumulation

- Add ray jittering to reduce banding artifacts
- Implement temporal accumulation to improve image quality over multiple frames

# Step 4:

Full Monte Carlo Volume Raycasting (optional, stretch goal)

- Implement Monte Carlo sampling techniques to improve the realism of the volume rendering.
- Explore advanced lighting models and scattering effects within the volume.
- Optimize performance to maintain interactive frame rates.

## Sidetracks

- Async parallel data loading
- Windows Release cppcoro coroutine ABI mismatch

# Current Status 
- At Step 4