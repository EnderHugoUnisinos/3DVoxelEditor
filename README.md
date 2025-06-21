# Editor de Voxels - Fundamentos de Computação Gráfica 2025/1


## 📂 Estrutura do Repositório

```plaintext
📂 3DVoxelEditor
├── 📄 CMakeLists.txt           # Configuração do CMake para compilar o projeto       
├── 📄 README.md                # Este arquivo, com a documentação do repositório
├── 📂 assets
│   └── 📂 block_tex
│       ├── empty.png
│       ├── frosted_ice_0.png
│       ├── glass.png
│       ├── ...
├── 📂 bin
├── 📂 common                   # Código reutilizável entre os projetos
│   └── glad.c
├── 📂 include                  # Cabeçalhos e bibliotecas de terceiros
│   ├── 📂 glad                 # Cabeçalhos da GLAD (OpenGL Loader)
│   │   ├── glad.h
│   │   └── 📂 KHR              # Diretório com cabeçalhos da Khronos (GLAD)
│   │       └── khrplatform.h
│   └── stb_image.h
└── 📂 src                      # Código-fonte
    ├── VoxelEditor.cpp
    └── voxel_grid.dat
```
