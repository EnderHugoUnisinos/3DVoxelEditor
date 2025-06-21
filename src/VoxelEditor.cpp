#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <fstream>

// STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace std;

// Dimensões da janela
const GLuint WIDTH = 1200, HEIGHT = 800;

// Variáveis globais de controle da câmera (posição, direção e orientação)
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 20.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
float yaw = -90.0f;
float pitch = 0.0f;
bool firstMouse = true;
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;
float fov = 45.0f;

// Controle de tempo entre frames
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// IDs de shader e VAO
GLuint shaderID, VAO;
GLFWwindow* window;

struct Voxel {
    glm::vec3 pos;
    float fatorEscala;
    bool visivel = true, selecionado = false;
    int texID;
};

int selecaoX = 0, selecaoY = 0, selecaoZ = 0;
const int TAM = 10;
Voxel grid[TAM][TAM][TAM];

// Lista de texturas
const int NUM_TEXTURES = 9;
vector<string> textureNames = {
    "Moss Block", "Glass", "Blackstone Bricks", "Sponge",
    "Honey Block", "Loom", "Packed Mud", "Frosted Ice", "Selection"
};

// IDs das texturas
GLuint texIDList[NUM_TEXTURES];

// Código do Vertex Shader
const GLchar* vertexShaderSource = R"glsl(
    #version 450
    layout (location = 0) in vec3 position;
    layout (location = 1) in vec2 texc;
    
    uniform mat4 view;
    uniform mat4 proj;
    uniform mat4 model;
    out vec2 tex_coord;
    void main()
    {
        tex_coord = vec2(texc.s, 1.0 - texc.t);
        gl_Position = proj * view * model * vec4(position, 1.0);
    }
)glsl";

// Código do Fragment Shader
const GLchar* fragmentShaderSource = R"glsl(
    #version 450
    in vec2 tex_coord;
    out vec4 color;
    uniform sampler2D tex_buff;
    void main()
    {
        color = texture(tex_buff, tex_coord);
    }
)glsl";

// Protótipos de funções
int loadTexture(string filePath);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void processInput(GLFWwindow* window);
void especificaVisualizacao();
void especificaProjecao();
void transformaObjeto(float xpos, float ypos, float zpos, 
                      float xrot, float yrot, float zrot, 
                      float sx, float sy, float sz);
GLuint setupShader();
GLuint setupGeometry();
void saveGrid(const char* filename);
void loadGrid(const char* filename);
void renderUI();
void printInstructions();
void clearScreen(); 

// Limpa o console
void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// Atualiza o viewport ao redimensionar a janela
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// Callback para movimentação do mouse
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.05f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);

    glm::vec3 right = glm::normalize(glm::cross(cameraFront, glm::vec3(0.0, 1.0, 0.0)));
    cameraUp = glm::normalize(glm::cross(right, cameraFront));
}

// Callback de scroll - altera o FOV (zoom)
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (fov >= 1.0f && fov <= 120.0f)
        fov -= yoffset;
    if (fov <= 1.0f) fov = 1.0f;
    if (fov >= 120.0f) fov = 120.0f;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    //Troca visibilidade do voxel
    if (key == GLFW_KEY_DELETE && action == GLFW_PRESS) {
        grid[selecaoY][selecaoX][selecaoZ].visivel = false;
    }
    if ((key == GLFW_KEY_V || key == GLFW_KEY_ENTER) && action == GLFW_PRESS) {
        grid[selecaoY][selecaoX][selecaoZ].visivel = true;
    }

    //Altera textura do voxel
    if (key == GLFW_KEY_T && action == GLFW_PRESS) {
        int& texID = grid[selecaoY][selecaoX][selecaoZ].texID;
        texID = (texID + 1) % (NUM_TEXTURES - 1); // Skip selection texture
        cout << "Textura alterada para: " << textureNames[texID] << endl;
    }
    else if (action == GLFW_PRESS && key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        int num = key - GLFW_KEY_1; // Converte para índice 0-8
        if (num < NUM_TEXTURES - 1) { // Verifica se o índice é válido
            int& texID = grid[selecaoY][selecaoX][selecaoZ].texID;
            texID = num;
            cout << "Textura alterada para: " << textureNames[texID] << endl;
        }
        else {
            cout << "Erro: Textura " << num + 1 << " indisponível (max: " << NUM_TEXTURES - 1 << ")" << endl;
        }
    }
    // Salva e carrega a grid
    if (key == GLFW_KEY_S && action == GLFW_PRESS && (mode & GLFW_MOD_CONTROL)) {
        saveGrid("voxel_grid.dat");
        cout << "Grid salva!" << endl;
    }
    if (key == GLFW_KEY_L && action == GLFW_PRESS && (mode & GLFW_MOD_CONTROL)) {
        loadGrid("voxel_grid.dat");
        cout << "Grid carregada!" << endl;
    }

    // Reseta a grid
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        for (int y = 0; y < TAM; y++) {
            for (int x = 0; x < TAM; x++) {
                for (int z = 0; z < TAM; z++) {
                    grid[y][x][z].visivel = false;
                    grid[y][x][z].texID = 0;
                }
            }
        }
        cout << "Grid resetada!" << endl;
    }

    // Move a seleção
    if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
        if (selecaoX + 1 < TAM) {
            grid[selecaoY][selecaoX][selecaoZ].selecionado = false;
            selecaoX++;
            grid[selecaoY][selecaoX][selecaoZ].selecionado = true;
        }
    }
    if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
        if (selecaoX - 1 >= 0) {
            grid[selecaoY][selecaoX][selecaoZ].selecionado = false;
            selecaoX--;
            grid[selecaoY][selecaoX][selecaoZ].selecionado = true;
        }
    }
    if (key == GLFW_KEY_UP && action == GLFW_PRESS) {
        if (selecaoY + 1 < TAM) {
            grid[selecaoY][selecaoX][selecaoZ].selecionado = false;
            selecaoY++;
            grid[selecaoY][selecaoX][selecaoZ].selecionado = true;
        }
    }
    if (key == GLFW_KEY_DOWN && action == GLFW_PRESS) {
        if (selecaoY - 1 >= 0) {
            grid[selecaoY][selecaoX][selecaoZ].selecionado = false;
            selecaoY--;
            grid[selecaoY][selecaoX][selecaoZ].selecionado = true;
        }
    }
    if (key == GLFW_KEY_PAGE_UP && action == GLFW_PRESS) {
        if (selecaoZ + 1 < TAM) {
            grid[selecaoY][selecaoX][selecaoZ].selecionado = false;
            selecaoZ++;
            grid[selecaoY][selecaoX][selecaoZ].selecionado = true;
        }
    }
    if (key == GLFW_KEY_PAGE_DOWN && action == GLFW_PRESS) {
        if (selecaoZ - 1 >= 0) {
            grid[selecaoY][selecaoX][selecaoZ].selecionado = false;
            selecaoZ--;
            grid[selecaoY][selecaoX][selecaoZ].selecionado = true;
        }
    }
}

// Processa as teclas pressionadas para movimentar a câmera no espaço 3D
void processInput(GLFWwindow* window) {
    float cameraSpeed = 5.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        cameraSpeed *= 5;
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}

// Define a matriz de visualização usando a posição e direção da câmera
void especificaVisualizacao() {
    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    GLuint loc = glGetUniformLocation(shaderID, "view");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(view));
}

// Define a matriz de projeção perspectiva com base no FOV
void especificaProjecao() {
    glm::mat4 proj = glm::perspective(glm::radians(fov), (float)WIDTH / HEIGHT, 0.1f, 100.0f);
    GLuint loc = glGetUniformLocation(shaderID, "proj");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(proj));
}

void transformaObjeto(float xpos, float ypos, float zpos, 
                      float xrot, float yrot, float zrot, 
                      float sx, float sy, float sz) {
    glm::mat4 transform = glm::mat4(1.0f);

    // especifica as transformações sobre o objeto - model
    transform = glm::translate(transform, glm::vec3(xpos, ypos, zpos));
    transform = glm::rotate(transform, glm::radians(xrot), glm::vec3(1, 0, 0));
    transform = glm::rotate(transform, glm::radians(yrot), glm::vec3(0, 1, 0));
    transform = glm::rotate(transform, glm::radians(zrot), glm::vec3(0, 0, 1));
    transform = glm::scale(transform, glm::vec3(sx, sy, sz));
    
    // Envia os dados para o shader
    GLuint loc = glGetUniformLocation(shaderID, "model");
    glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(transform));
}

// Compila shaders e cria o programa de shader
GLuint setupShader() {
    GLint success;
    GLchar infoLog[512];

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        cout << "Erro no Vertex Shader:\n" << infoLog << endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        cout << "Erro no Fragment Shader:\n" << infoLog << endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        cout << "Erro na linkagem do Shader Program:\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// Cria o VAO com os vértices e coordenadas de textura do cubo
GLuint setupGeometry() {
    GLfloat vertices[] = {
        // Layout do vértice: x, y, z, s, t
        // Face da frente (z = +0.5)
        0.5,  0.5,  0.5, 1.0, 1.0,
        0.5, -0.5,  0.5, 1.0, 0.0,
        -0.5, -0.5,  0.5, 0.0, 0.0,
        0.5,  0.5,  0.5, 1.0, 1.0,
        -0.5, -0.5,  0.5, 0.0, 0.0,
        -0.5,  0.5,  0.5, 0.0, 1.0,

        // Face de trás (z = -0.5)
        -0.5,  0.5, -0.5, 1.0, 1.0,
        -0.5, -0.5, -0.5, 1.0, 0.0,
        0.5, -0.5, -0.5, 0.0, 0.0,
        -0.5,  0.5, -0.5, 1.0, 1.0,
        0.5, -0.5, -0.5, 0.0, 0.0,
        0.5,  0.5, -0.5, 0.0, 1.0,

        // Face esquerda (x = -0.5)
        -0.5,  0.5,  0.5, 0.0, 1.0,
        -0.5, -0.5,  0.5, 0.0, 0.0,
        -0.5, -0.5, -0.5, 1.0, 0.0,
        -0.5,  0.5,  0.5, 0.0, 1.0,
        -0.5, -0.5, -0.5, 1.0, 0.0,
        -0.5,  0.5, -0.5, 1.0, 1.0,

        // Face direita (x = +0.5)
        0.5,  0.5, -0.5, 1.0, 1.0,
        0.5, -0.5, -0.5, 1.0, 0.0,
        0.5, -0.5,  0.5, 0.0, 0.0,
        0.5,  0.5, -0.5, 1.0, 1.0,
        0.5, -0.5,  0.5, 0.0, 0.0,
        0.5,  0.5,  0.5, 0.0, 1.0,

        // Face de baixo (y = -0.5)
        -0.5, -0.5,  0.5, 0.0, 1.0,
        0.5, -0.5,  0.5, 1.0, 1.0,
        0.5, -0.5, -0.5, 1.0, 0.0,
        -0.5, -0.5,  0.5, 0.0, 1.0,
        0.5, -0.5, -0.5, 1.0, 0.0,
        -0.5, -0.5, -0.5, 0.0, 0.0,

        // Face de cima (y = +0.5)
        -0.5,  0.5, -0.5, 0.0, 0.0,
        0.5,  0.5, -0.5, 1.0, 0.0,
        0.5,  0.5,  0.5, 1.0, 1.0,
        -0.5,  0.5, -0.5, 0.0, 0.0,
        0.5,  0.5,  0.5, 1.0, 1.0,
        -0.5,  0.5,  0.5, 0.0, 1.0
    };

    GLuint VBO, vao;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &VBO);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 1 atributo - coordenadas x, y, z
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

     // 2 atributo - coordenadas de textura s, t 
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return vao;
}

// Carrega uma textura de arquivo
int loadTexture(string filePath) {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &nrChannels, 0);

    if (data) {
        if (nrChannels == 3) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        } else if (nrChannels == 4) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        }
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        cout << "Falha ao carregar textura: " << filePath << endl;
        unsigned char defaultTex[] = {
            255, 0, 255, 255,   0, 0, 0, 255,
            0, 0, 0, 255,       255, 0, 255, 255
        };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, defaultTex);
    }

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return texID;
}

// Salva o estado da grid em um arquivo .dat
void saveGrid(const char* filename) {
    ofstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "Falha ao salvar grid!" << endl;
        return;
    }

    for (int y = 0; y < TAM; y++) {
        for (int x = 0; x < TAM; x++) {
            for (int z = 0; z < TAM; z++) {
                file.write(reinterpret_cast<char*>(&grid[y][x][z].texID), sizeof(int));
                file.write(reinterpret_cast<char*>(&grid[y][x][z].visivel), sizeof(bool));
            }
        }
    }

    file.close();
}

// Carrega o estado da grid de um arquivo .dat
void loadGrid(const char* filename) {
    ifstream file(filename, ios::binary);
    if (!file.is_open()) {
        cerr << "Falha ao carregar grid!" << endl;
        return;
    }

    for (int y = 0; y < TAM; y++) {
        for (int x = 0; x < TAM; x++) {
            for (int z = 0; z < TAM; z++) {
                file.read(reinterpret_cast<char*>(&grid[y][x][z].texID), sizeof(int));
                file.read(reinterpret_cast<char*>(&grid[y][x][z].visivel), sizeof(bool));
            }
        }
    }

    file.close();
}

// Imprime informações
void renderUI() {
    static bool firstFrame = true;
    if (firstFrame) {
        printInstructions();
        firstFrame = false;
    }

    static int frameCount = 0;
    if (frameCount++ % 60 == 0) {
        clearScreen();  //limpa o console
        printInstructions();
        
        cout << "\n=== Editor de Voxel ===" << endl;
        cout << "Posicao: (" << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << ")" << endl;
        cout << "Selecao: (" << selecaoX << ", " << selecaoY << ", " << selecaoZ << ")" << endl;
        cout << "Voxel: " << (grid[selecaoY][selecaoX][selecaoZ].visivel ? "Visivel" : "Oculto");
        cout << " | Textura: " << textureNames[grid[selecaoY][selecaoX][selecaoZ].texID] << endl;
    }
}

// Imprime controles
void printInstructions() {
    cout << "=== Controles ===" << endl;
    cout << "WASD: Mover camera" << endl;
    cout << "Mouse: Olhar em volta" << endl;
    cout << "Scroll: Zoom" << endl;
    cout << "Setas: Mover selecao em X/Y" << endl;
    cout << "Page Up/Down: Mover selecao em Z" << endl;
    cout << "T: Mudar textura" << endl;
    cout << "V: Colocar voxel" << endl;
    cout << "Delete: Apagar/Esconder voxel" << endl;
    cout << "Ctrl + S: Salvar grid" << endl;
    cout << "Ctrl + L: Carregar grid" << endl;
    cout << "R: Resetar grid" << endl;
    cout << "ESC: Sair" << endl;
}

// Função principal
int main() {
    if (!glfwInit()) {
        cerr << "Falha ao inicializar GLFW" << endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Editor de Voxel - Ender Hugo", nullptr, nullptr);
    if (!window) {
        cerr << "Falha ao criar janela GLFW" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "Falha ao inicializar GLAD" << endl;
        return -1;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shaderID = setupShader();
    VAO = setupGeometry();

    texIDList[0] = loadTexture("../assets/block_tex/moss_block.png");
    texIDList[1] = loadTexture("../assets/block_tex/glass.png");
    texIDList[2] = loadTexture("../assets/block_tex/polished_blackstone_bricks.png");
    texIDList[3] = loadTexture("../assets/block_tex/sponge.png");
    texIDList[4] = loadTexture("../assets/block_tex/honey_block_top.png");
    texIDList[5] = loadTexture("../assets/block_tex/loom_bottom.png");
    texIDList[6] = loadTexture("../assets/block_tex/packed_mud.png");
    texIDList[7] = loadTexture("../assets/block_tex/frosted_ice_0.png");
    texIDList[8] = loadTexture("../assets/block_tex/selected.png");

    for (int y = 0, yPos = -TAM/2; y < TAM; y++, yPos += 1.0f) {
        for (int x = 0, xPos = -TAM/2; x < TAM; x++, xPos += 1.0f) {
            for (int z = 0, zPos = -TAM/2; z < TAM; z++, zPos += 1.0f) {
                grid[y][x][z].pos = glm::vec3(xPos, yPos, zPos);
                grid[y][x][z].texID = 0;
                grid[y][x][z].fatorEscala = 0.98f;
                grid[y][x][z].visivel = false;
            }
        }
    }

    grid[selecaoY][selecaoX][selecaoZ].selecionado = true;
    grid[selecaoY][selecaoX][selecaoZ].visivel = true;
    grid[selecaoY][selecaoX][selecaoZ].texID = 1;

    glUseProgram(shaderID);
    glUniform1i(glGetUniformLocation(shaderID, "tex_buff"), 0);

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        especificaVisualizacao();
        especificaProjecao();

        glBindVertexArray(VAO);
        for (int x = 0; x < TAM; x++) {
            for (int y = 0; y < TAM; y++) {
                for (int z = 0; z < TAM; z++) {
                    if (grid[y][x][z].visivel) {
                        glBindTexture(GL_TEXTURE_2D, texIDList[grid[y][x][z].texID]);
                        transformaObjeto(
                            grid[y][x][z].pos.x, 
                            grid[y][x][z].pos.y, 
                            grid[y][x][z].pos.z,
                            0.0f, 0.0f, 0.0f,
                            grid[y][x][z].fatorEscala,
                            grid[y][x][z].fatorEscala,
                            grid[y][x][z].fatorEscala
                        );
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }
                }
            }
        }
        for (int x = 0; x < TAM; x++) {
            for (int y = 0; y < TAM; y++) {
                for (int z = 0; z < TAM; z++) {
                    if (grid[y][x][z].selecionado) {
                        glBindTexture(GL_TEXTURE_2D, texIDList[8]);
                        transformaObjeto(
                            grid[y][x][z].pos.x, 
                            grid[y][x][z].pos.y, 
                            grid[y][x][z].pos.z,
                            0.0f, 0.0f, 0.0f,
                            grid[y][x][z].fatorEscala * 1.05f,
                            grid[y][x][z].fatorEscala * 1.05f,
                            grid[y][x][z].fatorEscala * 1.05f
                        );
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                    }
                }
            }
        }

        renderUI();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(shaderID);
    glfwTerminate();
    return 0;
}