# TODO

---

- Change pipeline layout to shared pointer
- Change dynamic rasterisation samples to fixed - recreate pipeline on change
- De-systemise a bit
- Fix debug window probably crashing if entities are removed and all ECS ids are cycled round
- Shadow mapping
- Mipmaps
- AA
- Skybox
- ssao?
- IBL D:
- Change VulkanEngine to global scope?
- Ray tracing?
- Switch from tinygltf -> [fastgltf](https://github.com/spnda/fastgltf)
- Multithreading asset loading?

### Small Optimisations (low prio.)

- Change normal matrix to use mat3 with correct alignment
- Separate systems into folder structure?
- Separate wireframe mode into another pipeline to increase performance

