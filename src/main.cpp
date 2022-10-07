//     Universidade Federal do Rio Grande do Sul
//             Instituto de Informática
//       Departamento de Informática Aplicada
//
//    INF01047 Fundamentos de Computaçãoo Gráfica
//               Prof. Eduardo Gastal
//
//                   Trabalho Final
//

// Arquivos "headers" padr�es de C podem ser inclu�dos em um
// programa C++, sendo necess�rio somente adicionar o caractere
// "c" antes de seu nome, e remover o sufixo ".h". Exemplo:
//    #include <stdio.h> // Em C
//  vira
//    #include <cstdio> // Em C++
//
#include <cmath>
#include <ctime>
#include <cstdio>
#include <cstdlib>

// Headers abaixo sao especificos de C++
#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <vector>
#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>   // Criacao de contexto OpenGL 3.3
#include <GLFW/glfw3.h>  // Criacao de janelas do sistema operacional

// Headers da biblioteca GLM: criacao de matrizes e vetores.
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>
#include <stb_image.h>

// Headers locais, definidos na pasta "include/"
#include "utils.h"
#include "matrices.h"
#include "collisions.h"

#define PI 3.14159265359

// Estrutura que representa um modelo geométrico carregado a partir de um
// arquivo ".obj". Veja https://en.wikipedia.org/wiki/Wavefront_.obj_file .
struct ObjModel
{
    tinyobj::attrib_t                 attrib;
    std::vector<tinyobj::shape_t>     shapes;
    std::vector<tinyobj::material_t>  materials;

    // Este construtor le o modelo de um arquivo utilizando a biblioteca tinyobjloader.
    // Veja: https://github.com/syoyo/tinyobjloader
    ObjModel(const char* filename, const char* basepath = NULL, bool triangulate = true)
    {
        printf("Carregando modelo \"%s\"... ", filename);

        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename, basepath, triangulate);

        if (!err.empty())
            fprintf(stderr, "\n%s\n", err.c_str());

        if (!ret)
            throw std::runtime_error("Erro ao carregar modelo.");

        printf("OK.\n");
    }
};

// Declaracao de funcoes utilizadas para pilha de matrizes de modelagem.
void PushMatrix(glm::mat4 M);
void PopMatrix(glm::mat4& M);

// Declaracao de varias funcoes utilizadas em main().
void BuildTrianglesAndAddToVirtualScene(ObjModel*); // Constroi representacao de um ObjModel como malha de triangulos para renderizacao
void ComputeNormals(ObjModel* model); // Computa normais de um ObjModel, caso nao existam.
void LoadShadersFromFiles(); // Carrega os shaders de vertice e fragmento, criando um programa de GPU
void LoadTextureImage(const char* filename); // Funcao que carrega imagens de textura
void DrawVirtualObject(const char* object_name); // Desenha um objeto armazenado em g_VirtualScene
GLuint LoadShader_Vertex(const char* filename);   // Carrega um vertex shader
GLuint LoadShader_Fragment(const char* filename); // Carrega um fragment shader
void LoadShader(const char* filename, GLuint shader_id); // Funcao utilizada pelas duas acima
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id); // Cria um programa de GPU
void PrintObjModelInfo(ObjModel*); // Funcao para debugging

// Declaracao de funcoes auxiliares para renderizar texto dentro da janela
// OpenGL. Estas funcoes estao definidas no arquivo "textrendering.cpp".
void TextRendering_Init();
float TextRendering_LineHeight(GLFWwindow* window);
float TextRendering_CharWidth(GLFWwindow* window);
void TextRendering_PrintString(GLFWwindow* window, const std::string &str, float x, float y, float scale = 1.0f);

// Funcoes callback para comunicacao com o sistema operacional e interacao do usuario.
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
void ErrorCallback(int error, const char* description);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);

//Funcao que exibe na tela uma mensagem (se o usuário ganhou ou perdeu o jogo)
glm::vec4 TextMessage(char* message);

// Funcoes que criam as paredes e chão das salas
RoomWallModel sceneWalls[4];
RoomWallModel CreateWallX(float positionX, float positionY, float positionZ, float scaleX, float scaleZ, int objType);  // parede
RoomWallModel CreateWallY(float positionX, float positionY, float positionZ, float scaleY, float scaleZ, int objType);  // parede
RoomWallModel CreateFloor(float positionX, float positionY, float positionZ, float scaleX, float scaleY, int objType);  // chao

//Funcoes que criam um objeto pequeno (cubo) para pegar os objetos nas salas
RoomWallModel MakeGetObj(float positionX, float positionY, float positionZ);
void UpdateGetObj(RoomWallModel &shot, float timeElapsed);
void DrawGetObj(RoomWallModel shot);

std::vector<RoomWallModel> sceneGetObj;

float prevCubeTime = -1;

//Struct que armazena os dados necessarios para renderizar cada objeto da cena
struct SceneObject
{
    std::string  name;        // Nome do objeto
    void*        first_index; // Indice do primeiro vertice dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    int          num_indices; // Numero de indices do objeto dentro do vetor indices[] definido em BuildTrianglesAndAddToVirtualScene()
    GLenum       rendering_mode; // Modo de rasterizacao (GL_TRIANGLES, GL_TRIANGLE_STRIP, etc.)
    GLuint       vertex_array_object_id; // ID do VAO onde estao armazenados os atributos do modelo
    glm::vec3    bbox_min; // Axis-Aligned Bounding Box do objeto
    glm::vec3    bbox_max;
};


// A cena virtual e uma lista de objetos nomeados, guardados em um dicionario
// (map).  Veja dentro da funcao BuildTrianglesAndAddToVirtualScene() como que sao incluidos
// objetos dentro da variavel g_VirtualScene, e veja na funcao main() como
// estes sao acessados.
std::map<std::string, SceneObject> g_VirtualScene;

// Pilha que guardara as matrizes de modelagem.
std::stack<glm::mat4>  g_MatrixStack;

// Razao de proporcao da janela (largura/altura). Veja funcao FramebufferSizeCallback().
float g_ScreenRatio = 1.0f;

// Angulos de Euler que controlam a rotacao de um cubo
float g_AngleX = 0.0f;
float g_AngleY = 0.0f;
float g_AngleZ = 0.0f;

// "g_LeftMouseButtonPressed = true" se o usuurio esta com o botao esquerdo do mouse
// pressionado no momento atual. Veja funcao MouseButtonCallback().
bool g_LeftMouseButtonPressed = false;
bool g_RightMouseButtonPressed = false; // Analogo para botao direito do mouse
bool g_MiddleMouseButtonPressed = false; // Analogo para botao do meio do mouse

// Variaveis que definem a camera em coordenadas esfericas, controladas pelo
// usuario atraves do mouse (veja funcao CursorPosCallback()). A posicao
// efetiva da camera e calculada dentro da funcao main(), dentro do loop de
// renderizacao.
float g_CameraTheta = PI; // angulo no plano ZX em relacao ao eixo Z
float g_CameraPhi = 0.0f;   // angulo em relacao ao eixo Y
float g_CameraDistance = 3.5f; // Distancia da camera para a origem

// Variavel que controla o tipo de projecao utilizada: perspectiva ou ortogr�fica.
bool g_UsePerspectiveProjection = true;

// Variavel que controla se o texto informativo serao mostrado na tela.
bool g_ShowInfoText = true;

// Variaveis que definem um programa de GPU (shaders). Veja funcao LoadShadersFromFiles().
GLuint vertex_shader_id;
GLuint fragment_shader_id;
GLuint program_id = 0;
GLint model_uniform;
GLint view_uniform;
GLint projection_uniform;
GLint bbox_min_uniform;
GLint object_id_uniform;
GLint plane_type_uniform;
GLint bbox_max_uniform;

// Numero de texturas carregadas pela funcao LoadTextureImage()
GLuint g_NumLoadedTextures = 0;

float depth = 25.0;

GLFWwindow* window;

// Atualiza a posicao da camera de acordo com as entradas do usuario
void updateCameraPosition(glm::vec4 &camera_view_vector);

bool pressed = false;
bool key_w_pressed = false;
bool key_a_pressed = false;
bool key_s_pressed = false;
bool key_d_pressed = false;
bool key_space_pressed = false;

glm::vec4 camera_movement = glm::vec4(0.0f, 0.0f, 2.5f, 0.0f);

int main(int argc, char* argv[])
{
    // Inicializamos a biblioteca GLFW, utilizada para criar uma janela do
    // sistema operacional, onde poderemos renderizar com OpenGL.
    int success = glfwInit();
    if (!success)
    {
        fprintf(stderr, "ERROR: glfwInit() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos o callback para impressao de erros da GLFW no terminal
    glfwSetErrorCallback(ErrorCallback);

    // Pedimos para utilizar OpenGL versao 3.3 (ou superior)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    // Pedimos para utilizar o perfil "core", isto e, utilizaremos somente as
    // funcoes modernas de OpenGL.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Criamos uma janela do sistema operacional, com 900 colunas e 700 linhas
    // de pixels, e com titulo "INF01047 ...".
    window = glfwCreateWindow(900, 700, "INF01047 - TRABALHO FINAL", NULL, NULL);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (!window)
    {
        glfwTerminate();
        fprintf(stderr, "ERROR: glfwCreateWindow() failed.\n");
        std::exit(EXIT_FAILURE);
    }

    // Definimos a funcao de callback que sera chamada sempre que o usuario
    // pressionar alguma tecla do teclado ...
    glfwSetKeyCallback(window, KeyCallback);
    // ... ou movimentar o cursor do mouse em cima da janela ...
    glfwSetCursorPosCallback(window, CursorPosCallback);

    // Indicamos que as chamadas OpenGL deverao renderizar nesta janela
    glfwMakeContextCurrent(window);

    // Carregamento de todas funcoes definidas por OpenGL 3.3, utilizando a
    // biblioteca GLAD.
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

    // Definimos a funcao de callback que sera chamada sempre que a janela for
    // redimensionada, por consequencia alterando o tamanho do "framebuffer"
    // (regiao de memoria onde sao armazenados os pixels da imagem).
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    FramebufferSizeCallback(window, 900, 700); // Forcamos a chamada do callback acima, para definir g_ScreenRatio.

    // Imprimimos no terminal informacoes sobre a GPU do sistema
    const GLubyte *vendor      = glGetString(GL_VENDOR);
    const GLubyte *renderer    = glGetString(GL_RENDERER);
    const GLubyte *glversion   = glGetString(GL_VERSION);
    const GLubyte *glslversion = glGetString(GL_SHADING_LANGUAGE_VERSION);

    printf("GPU: %s, %s, OpenGL %s, GLSL %s\n", vendor, renderer, glversion, glslversion);

    // Carregamos os shaders de vertices e de fragmentos que serao utilizados
    // para renderizacao. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf
    LoadShadersFromFiles();

    // Carregamos duas imagens para serem utilizadas como textura
    LoadTextureImage("../../data/wall_texture3.jpg"); // TextureImage0
    LoadTextureImage("../../data/floor.jpg"); // TextureImage1
    ObjModel cubemodel("../../data/cube.obj");
    ComputeNormals(&cubemodel);
    BuildTrianglesAndAddToVirtualScene(&cubemodel);


// Objetos em cada sala
    // SALA 1:

    ObjModel bedmodel("../../data/krovat-2.obj", "../../data/krovat-2.mtl");  //bed
    ComputeNormals(&bedmodel);
    BuildTrianglesAndAddToVirtualScene(&bedmodel);

    ObjModel wardrobemodel("../../data/old_rustic_stand.obj", "../../data/old_rustic_stand.mtl");  //stand
    ComputeNormals(&wardrobemodel);
    BuildTrianglesAndAddToVirtualScene(&wardrobemodel);

    ObjModel mirrormodel("../../data/antique_standing_mirror.obj", "../../data/antique_standing_mirror.mtl"); //mirror
    ComputeNormals(&mirrormodel);
    BuildTrianglesAndAddToVirtualScene(&mirrormodel);

    ObjModel bookshelfmodel("../../data/Old_Dusty_Bookshelf.obj", "../../data/Old_Dusty_Bookshelf.mtl"); //bookshelf
    ComputeNormals(&bookshelfmodel);
    BuildTrianglesAndAddToVirtualScene(&bookshelfmodel);

    ObjModel tablemodel("../../data/table.obj", "../../data/table.mtl"); //table
    ComputeNormals(&tablemodel);
    BuildTrianglesAndAddToVirtualScene(&tablemodel);

    ObjModel seatmodel("../../data/seat.obj", "../../data/seat.mtl"); //seat
    ComputeNormals(&seatmodel);
    BuildTrianglesAndAddToVirtualScene(&seatmodel);

    ObjModel bigbenmodel("../../data/BiBe.obj", "../../data/BiBe.mtl");  //chair
    ComputeNormals(&bigbenmodel);
    BuildTrianglesAndAddToVirtualScene(&bigbenmodel);


    // SALA 2:

    ObjModel cabinetmodel("../../data/modern_cabinet_hutch.obj", "../../data/modern_cabinet_hutch.mtl"); //cabinet
    ComputeNormals(&cabinetmodel);
    BuildTrianglesAndAddToVirtualScene(&cabinetmodel);

    ObjModel oldtablemodel("../../data/old_table_obj.obj", "../../data/old_table_mtl.mtl"); //old table
    ComputeNormals(&oldtablemodel);
    BuildTrianglesAndAddToVirtualScene(&oldtablemodel);

    ObjModel benchmodel("../../data/bench.obj", "../../data/bench.mtl"); //bench
    ComputeNormals(&benchmodel);
    BuildTrianglesAndAddToVirtualScene(&benchmodel);

    ObjModel fridgemodel("../../data/fridge.obj"); //fridge
    ComputeNormals(&fridgemodel);
    BuildTrianglesAndAddToVirtualScene(&fridgemodel);

    ObjModel couch2model("../../data/U-shaped_sofa.obj", "../../data/U-shaped_sofa.mtl"); //sofa
    ComputeNormals(&couch2model);
    BuildTrianglesAndAddToVirtualScene(&couch2model);

    ObjModel chair1model("../../data/chair_1.obj", "../../data/chair_1.mtl"); //chair
    ComputeNormals(&chair1model);
    BuildTrianglesAndAddToVirtualScene(&chair1model);

    ObjModel knifemodel("../../data/Knife.obj", "../../data/Knife.mtl"); //knife
    ComputeNormals(&knifemodel);
    BuildTrianglesAndAddToVirtualScene(&knifemodel);


    // SALA 3:

    ObjModel planemodel("../../data/plane.obj");
    ComputeNormals(&planemodel);
    BuildTrianglesAndAddToVirtualScene(&planemodel);

    ObjModel chairmodel("../../data/chair.obj", "../../data/chair.mtl");  //chair
    ComputeNormals(&chairmodel);
    BuildTrianglesAndAddToVirtualScene(&chairmodel);

    ObjModel roundmirrormodel("../../data/round_mirror.obj", "../../data/round_mirror.mtl"); //mirror
    ComputeNormals(&roundmirrormodel);
    BuildTrianglesAndAddToVirtualScene(&roundmirrormodel);

    ObjModel toiletmodel("../../data/SA_LD_Toilet.obj", "../../data/SA_LD_Toilet.mtl"); //toilet
    ComputeNormals(&toiletmodel);
    BuildTrianglesAndAddToVirtualScene(&toiletmodel);

    ObjModel closetmodel("../../data/Kitchen_1_Wardrobe.obj", "../../data/Kitchen_1_Wardrobe.mtl");
    ComputeNormals(&closetmodel);
    BuildTrianglesAndAddToVirtualScene(&closetmodel);

    ObjModel matmodel("../../data/mat.obj");
    ComputeNormals(&matmodel);
    BuildTrianglesAndAddToVirtualScene(&matmodel);

    ObjModel showermodel("../../data/shower.obj", "../../data/shower.mtl"); //shower
    ComputeNormals(&showermodel);
    BuildTrianglesAndAddToVirtualScene(&showermodel);

    ObjModel broommodel("../../data/broom.obj", "../../data/broom.mtl"); //broom
    ComputeNormals(&broommodel);
    BuildTrianglesAndAddToVirtualScene(&broommodel);


    if ( argc > 1 )
    {
        ObjModel model(argv[1]);
        BuildTrianglesAndAddToVirtualScene(&model);
    }

    // Inicializamos o codigo para renderizacao de texto.
    TextRendering_Init();

    // Habilitamos o Z-buffer. Veja slide 104-116 do documento Aula_09_Projecoes.pdf.
    glEnable(GL_DEPTH_TEST);

    srand(time(NULL));

    // Dicas para o usuario encontrar o objeto correto de cada sala
    printf("\n\nSALA 1: Descubra onde que ocorreu o crime! Procure pelo lugar em que os onibus sao vermelhos.");

    printf("\nSALA 2: Descubra qual foi a arma do crime! Procure por aquilo que perde o fio ao longo do tempo.");

    printf("\nSALA 3: Por fim, descubra quem foi o assassino do crime! Procure por um item que remeta a alguem que trabalha na casa.");

    // variaveis de controle
    int cont = 0;
    bool gameOver = false;
    bool first = true;
    bool second = false;
    bool third = false;
    bool changeToSecondRoom = false;
    bool changeToThirdRoom = false;

    // Ficamos em loop, renderizando, ate que o usuario feche a janela (esc)
    while (!glfwWindowShouldClose(window))
    {

        cont += 1;
        // Aqui executamos as operacoes de renderizacao

        // Definimos a cor do "fundo" do framebuffer como branco.  Tal cor e
        // definida como coeficientes RGBA: Red, Green, Blue, Alpha; isto e:
        // Vermelho, Verde, Azul, Alpha (valor de transparencia).
        // Conversaremos sobre sistemas de cores nas aulas de Modelos de Ilumina��o.
        //
        //           R     G     B     A
        glClearColor(0.0f, 0.0f, 0.2f, 1.0f);

        // "Pintamos" todos os pixels do framebuffer com a cor definida acima,
        // e tambem resetamos todos os pixels do Z-buffer (depth buffer).
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pedimos para a GPU utilizar o programa de GPU criado acima (contendo
        // os shaders de vertice e fragmentos).
        glUseProgram(program_id);

        // Computamos a posicao da camera utilizando coordenadas esfericas.  As
        // variaveis g_CameraDistance, g_CameraPhi, e g_CameraTheta sao
        // controladas pelo mouse do usuario. Veja as funcoes CursorPosCallback()
        // e ScrollCallback().
        float r = g_CameraDistance;
        float y = r*sin(g_CameraPhi);
        float z = r*cos(g_CameraPhi)*cos(g_CameraTheta);
        float x = r*cos(g_CameraPhi)*sin(g_CameraTheta);

        // Abaixo definimos as varaveis que efetivamente definem a camera virtual.
        // Veja slides 195-227 e 229-234 do documento Aula_08_Sistemas_de_Coordenadas.pdf.
        glm::vec4 camera_position_c  = glm::vec4(0.0f,0.0f,0.0f,1.0f) + camera_movement; 		// Ponto "c", centro da camera
        glm::vec4 camera_lookat_l    = glm::vec4(x,y,z,1.0f) + camera_movement; 				// Ponto "l", para onde a camera (look-at) estara sempre olhando
        glm::vec4 camera_view_vector = camera_lookat_l - camera_position_c; 					// Vetor "view", sentido para onde a camera esta virada
        glm::vec4 camera_up_vector   = glm::vec4(0.0f,1.0f,0.0f,0.0f); 							// Vetor "up" fixado para apontar para o "ceu" (eixo Y global)

        // Computamos a matriz "View" utilizando os parametros da camera para
        // definir o sistema de coordenadas da camera.  Veja slides 2-14, 184-190 e 236-242 do documento Aula_08_Sistemas_de_Coordenadas.pdf.
        glm::mat4 view = Matrix_Camera_View(camera_position_c, camera_view_vector, camera_up_vector);

        // Agora computamos a matriz de Projecao.
        glm::mat4 projection;

        // Note que, no sistema de coordenadas da camera, os planos near e far
        // estao no sentido negativo! Veja slides 176-204 do documento Aula_09_Projecoes.pdf.
        float nearplane = -0.1f;  // Posicao do "near plane"
        float farplane  = -100000.0f; // Posicao do "far plane"

        if (g_UsePerspectiveProjection)
        {
            // Projecao Perspectiva.
            // Para definicao do field of view (FOV), veja slides 205-215 do documento Aula_09_Projecoes.pdf.
            float field_of_view = 3.141592 / 3.0f;
            projection = Matrix_Perspective(field_of_view, g_ScreenRatio, nearplane, farplane);
        }
        else
        {
            // Projecao Ortografica.
            // Para definicao dos valores l, r, b, t ("left", "right", "bottom", "top"),
            // veja slides 219-224 do documento Aula_09_Projecoes.pdf.
            // Para simular um "zoom" ortografico, computamos o valor de "t"
            // utilizando a variavel g_CameraDistance.
            float t = 1.5f*g_CameraDistance/2.5f;
            float b = -t;
            float r = t*g_ScreenRatio;
            float l = -r;
            projection = Matrix_Orthographic(l, r, b, t, nearplane, farplane);
        }

        // Enviamos as matrizes "view" e "projection" para a placa de v�deo
        // (GPU). Veja o arquivo "shader_vertex.glsl", onde estas sao
        // efetivamente aplicadas em todos os pontos.
        glUniformMatrix4fv(view_uniform       , 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(projection_uniform , 1 , GL_FALSE , glm::value_ptr(projection));

        #define ROOM1    1
        #define PLANE    2
        #define ROOM2    3
        #define ROOM3    4
        #define LONDON   1
        #define KNIFE    3
        #define BROOM    4
        #define GET_OBJ  4
        #define FLOOR 1
        #define WALL  0


        if (first) {

            #define room1Height 8.0f
            #define room1Width 12.0f
            #define room1Depth 10.0f
            #define room1Begining 4.0f


            sceneWalls[0] = CreateWallX(0.0f, 0.0f, room1Begining, room1Width, room1Height, WALL);                               //Make back wall
            sceneWalls[1] = CreateWallX(0.0f, 0.0f, -(2 * room1Depth) + room1Begining, room1Width, room1Height, WALL);                               //Make front wall
            sceneWalls[2] = CreateWallY(room1Width, 0.0f, -room1Depth + room1Begining, room1Depth, room1Height, WALL);     // Make right wall
            sceneWalls[3] = CreateWallY(-room1Width, 0.0f, -room1Depth + room1Begining, room1Depth, room1Height, WALL);    // Make left wall
            CreateFloor(0.0f, -room1Height, -room1Depth + room1Begining, room1Width, room1Depth, FLOOR);        // Make the floor

            // SALA 1:  Objeto que deve ser encontrado é o bigben (lugar do crime: londres)

            glm::mat4 model = Matrix_Translate(-8.0f, -7.5f, -(0.8 * 12.0f)) * Matrix_Rotate_Y(-PI/1) * Matrix_Scale(5.5, 4.0, 3.5);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM1);
            DrawVirtualObject("krovat-2");

            model = Matrix_Translate(-3.0f, -7.5f, -(1.1 * 12.0f)) * Matrix_Rotate_Y(-PI/1) * Matrix_Scale(3.7, 3.7, 3.7);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM1);
            DrawVirtualObject("old_rustic_stand");

            model = Matrix_Translate(-9.0, -7.5f, -(0.2 * 12.0f)) * Matrix_Rotate_Y(-PI/2) * Matrix_Scale(2.5, 2.5, 2.5);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM1);
            DrawVirtualObject("antique_standing_mirror");

            model = Matrix_Translate(7.0f, -7.5f, -(0.3 * 12.0f)) * Matrix_Rotate_Y(-PI/0.68) * Matrix_Scale(2.5, 2.5, 2.5);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM1);
            DrawVirtualObject("Old_Dusty_Bookshelf");

            model = Matrix_Translate(4.0f, -7.5f, -(0.8 * 12.0f)) * Matrix_Rotate_Y(-PI/1) * Matrix_Scale(2.8, 2.8, 2.8);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM1);
            DrawVirtualObject("table");

            model = Matrix_Translate(4.0f, -4.5f, -(0.9 * 12.0f)) * Matrix_Rotate_Y(-PI/2.0) * Matrix_Scale(10.0, 10.0, 10.0);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, LONDON);
            DrawVirtualObject("Big_Ben");

            model = Matrix_Translate(6.0f, -7.5f, -(0.8 * 12.0f)) * Matrix_Rotate_Y(-PI/1) * Matrix_Scale(2.5, 2.5, 2.5);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM1);
            DrawVirtualObject("seat");

        }

        if(changeToSecondRoom) {
            first = false;

            #define room2Height 8.0f
            #define room2Width 16.0f
            #define room2Depth 16.0f
            #define room2Begining 4.0f

            sceneWalls[0] = CreateWallX(0.0f, 0.0f, room2Begining, room2Width, room2Height, WALL);                               //Make back wall
            sceneWalls[1] = CreateWallX(0.0f, 0.0f, -(2 * room2Depth) + room2Begining, room2Width, room2Height, WALL);                               //Make front wall
            sceneWalls[2] = CreateWallY(room2Width, 0.0f, -room2Depth + room2Begining, room2Depth, room2Height, WALL);     // Make right wall
            sceneWalls[3] = CreateWallY(-room2Width, 0.0f, -room2Depth + room2Begining, room2Depth, room2Height, WALL);    // Make left wall
            CreateFloor(0.0f, -room2Height, -room2Depth + room2Begining, room2Width, room2Depth, FLOOR);        // Make the floor

            // SALA 2: Objeto que deve ser encontrado é a faca (arma do crime)

            glm::mat4 model = Matrix_Translate(9.0f, -7.5f, -(2.0 * 12.0f)) * Matrix_Rotate_Y(-PI/1) * Matrix_Scale(4.5, 4.5, 4.0);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM2);
            DrawVirtualObject("cabinet_hutch");

            model = Matrix_Translate(3.0f, -7.5f, -(0.7 * 12.0f)) * Matrix_Rotate_Y(-PI/2) * Matrix_Scale(1.0, 3.5, 1.0);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM2);
            DrawVirtualObject("old_table");

            model = Matrix_Translate(7.0f, -7.5f, -(1.0 * 12.0f)) * Matrix_Rotate_Y(-PI/2) * Matrix_Scale(1.0, 0.6, 1.5);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM2);
            DrawVirtualObject("bench");

            model = Matrix_Translate(7.0f, -7.5f, -(0.6 * 12.0f)) * Matrix_Rotate_Y(-PI/2) * Matrix_Scale(1.0, 0.6, 1.5);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM2);
            DrawVirtualObject("bench");

            model = Matrix_Translate(13.0f, -7.5f, -(1.5 * 12.0f)) * Matrix_Rotate_Y(-PI/1) * Matrix_Scale(2.5, 3.5, 2.5);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM2);
            DrawVirtualObject("fridge");

            model = Matrix_Translate(-5.0f, -7.5f, -(1.6 * 12.0f)) * Matrix_Rotate_Y(-PI/25) * Matrix_Scale(1.2, 1.2, 1.0);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM2);
            DrawVirtualObject("Sofa_Cube");

            model = Matrix_Translate(-17.0f, -7.5f, -(0.1 * 12.0f)) * Matrix_Rotate_Y(-PI/100) * Matrix_Scale(4.0, 4.0, 4.0);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM2);
            DrawVirtualObject("armchair");

            model = Matrix_Translate(7.0f, -4.5f, -(0.7 * 12.0f)) * Matrix_Rotate_Y(-PI/1.0) * Matrix_Scale(0.2, 0.2, 0.2);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, KNIFE);
            DrawVirtualObject("knife");

        }

        if(changeToThirdRoom) {
            first = false;

            #define room3Height 8.0f
            #define room3Width 8.0f
            #define room3Depth 8.0f
            #define room3Begining 4.0f

            sceneWalls[0] = CreateWallX(0.0f, 0.0f, room3Begining, room3Width, room3Height, WALL);                               //Make back wall
            sceneWalls[1] = CreateWallX(0.0f, 0.0f, -(2 * room3Depth) + room3Begining, room3Width, room3Height, WALL);                               //Make front wall
            sceneWalls[2] = CreateWallY(room3Width, 0.0f, -room3Depth + room3Begining, room3Depth, room3Height, WALL);     // Make right wall
            sceneWalls[3] = CreateWallY(-room3Width, 0.0f, -room3Depth + room3Begining, room3Depth, room3Height, WALL);    // Make left wall
            CreateFloor(0.0f, -room3Height, -room3Depth + room3Begining, room3Width, room3Depth, FLOOR);        // Make the floor

            // SALA 3: Objeto que deve ser encontrado é a vassoura (assassino: faxineiro)

            glm::mat4 model = Matrix_Translate(-3.0f, -1.0f, -(0.8 * 12.0f)) * Matrix_Rotate_Z(-PI/2.0) * Matrix_Rotate_X(-PI/2.2) * Matrix_Scale(3.0, 3.0, 3.0);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM3);
            DrawVirtualObject("round_mirror");

            model = Matrix_Translate(5.0f, -6.0f, -(0.7 * 12.0f)) * Matrix_Rotate_Y(-PI/2) * Matrix_Scale(0.6, 0.6, 0.6);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM3);
            DrawVirtualObject("Toilet");

            model = Matrix_Translate(-3.0f, -7.0f, -(0.8 * 12.0f)) * Matrix_Rotate_Y(-PI/38) * Matrix_Scale(0.8, 0.8, 0.8);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM3);
            DrawVirtualObject("cabinet");

            model = Matrix_Translate(-6.0f, -7.5f, -(0.2 * 12.0f)) * Matrix_Rotate_Y(-PI/2) * Matrix_Scale(0.03, 0.03, 0.03);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM3);
            DrawVirtualObject("mat");

            model = Matrix_Translate(-7.0f, -2.0f, -(0.2 * 12.0f)) * Matrix_Rotate_Y(-PI/0.65) * Matrix_Scale(4.5, 6.5, 4.5);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, ROOM3);
            DrawVirtualObject("shower");

            model = Matrix_Translate(5.0f, -7.0f, -(0.3 * 12.0f)) * Matrix_Rotate_Y(-PI/1) * Matrix_Scale(2.0, 2.0, 2.0);
            glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
            glUniform1i(object_id_uniform, BROOM);
            DrawVirtualObject("broom");

        }

        // Atualiza posicao da camera
        updateCameraPosition(camera_view_vector);

        float curTime = glfwGetTime();
        float elapsedTime = (prevCubeTime > 0) ? (curTime - prevCubeTime) : -1;

        char* winMessage = {"Parabens, voce ganhou o jogo e desvendou o assassinato!!!"};
        char* loseMessage = {"O tempo acabou e voce nao conseguiu desvender o assassinato, tente outra vez!!!"};

        // Se o usuário pressiona espaço, tenta pegar o objeto
        if (key_space_pressed == true) {
          RoomWallModel getObj = MakeGetObj(camera_position_c.x, -3.8, camera_position_c.z - 2);
          sceneGetObj.push_back(getObj);
        }

        int i = 0;
        // cada vez que o usario tenta pegar um obj, verifica se tem colisão
        for(int j = 0; j < sceneGetObj.size(); j++)
        {
          UpdateGetObj(sceneGetObj[i], elapsedTime);
          if (CollisionObj(3.7, 4.5, sceneGetObj[i]) && first)
          {
              changeToSecondRoom = true;
              second = true;
              first = false;
              i--;
          }
          else if (CollisionObj(6.0, 8.0, sceneGetObj[i]) && second)
          {
              changeToSecondRoom = false;
              changeToThirdRoom = true;
              third = true;
              second = false;
              i--;
          }
          else if (CollisionObj(4.9, 5.2, sceneGetObj[i]) && third)
          {
              third = false;
              i--;
              //Usuario encontrou o ultimo objeto, ganhando o jogo
              TextMessage(winMessage);
          }
          i++;
        }

        // Tempo limite foi atingido e o usuario perde o jogo
        if (cont == 50000 || gameOver){
            gameOver = true;
            TextMessage(loseMessage);
        }

        i = 0;
        for(int it = 0; it < sceneGetObj.size(); it++)
        {
          if(sceneGetObj[i].positionZ >= (room1Begining - room1Depth) * 2)
          {
            DrawGetObj(sceneGetObj[i]);
            i++;
          }
           else {
            sceneGetObj.erase(sceneGetObj.begin() + i);
          }
        }

        key_space_pressed = false;

        prevCubeTime = curTime;

        // O framebuffer onde OpenGL executa as operacoes de renderizacao n�o
        // e o mesmo que esta sendo mostrado para o usuario, caso contrario
        // seria poss�vel ver artefatos conhecidos como "screen tearing". A
        // chamada abaixo faz a troca dos buffers, mostrando para o usuario
        // tudo que foi renderizado pelas funcoes acima.
        // Veja o link: Veja o link: https://en.wikipedia.org/w/index.php?title=Multiple_buffering&oldid=793452829#Double_buffering_in_computer_graphics
        glfwSwapBuffers(window);

        // Verificamos com o sistema operacional se houve alguma interacao do
        // usuario (teclado, mouse, ...). Caso positivo, as funçoes de callback
        // definidas anteriormente usando glfwSet*Callback() serao chamadas
        // pela biblioteca GLFW.
        glfwPollEvents();
    }

    // Finalizamos o uso dos recursos do sistema operacional
    glfwTerminate();

    // Fim do programa
    return 0;
}

// Funcao que carrega uma imagem para ser utilizada como textura
void LoadTextureImage(const char* filename)
{
    printf("Carregando imagem \"%s\"... ", filename);

    int width;
    int height;
    int channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 3);

    if ( data == NULL )
    {
        fprintf(stderr, "ERROR: Cannot open image file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }

    printf("OK (%dx%d).\n", width, height);

    // Agora criamos objetos na GPU com OpenGL para armazenar a textura
    GLuint texture_id;
    GLuint sampler_id;
    glGenTextures(1, &texture_id);
    glGenSamplers(1, &sampler_id);

    // Veja slide 95-96 do documento Aula_20_Mapeamento_de_Texturas.pdf
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Parametros de amostragem da textura. Falaremos sobre eles em uma proxima aula.
    glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Agora enviamos a imagem lida do disco para a GPU
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

    GLuint textureunit = g_NumLoadedTextures;
    glActiveTexture(GL_TEXTURE0 + textureunit);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindSampler(textureunit, sampler_id);

    stbi_image_free(data);

    g_NumLoadedTextures += 1;
}

// Funcao que desenha um objeto armazenado em g_VirtualScene. Veja defini��o
// dos objetos na funcao BuildTrianglesAndAddToVirtualScene().
void DrawVirtualObject(const char* object_name)
{
    // "Ligamos" o VAO. Informamos que queremos utilizar os atributos de
    // vertices apontados pelo VAO criado pela funcao BuildTrianglesAndAddToVirtualScene(). Veja
    // comentarios detalhados dentro da definicao de BuildTrianglesAndAddToVirtualScene().
    glBindVertexArray(g_VirtualScene[object_name].vertex_array_object_id);

    // Setamos as variaveis "bbox_min" e "bbox_max" do fragment shader
    // com os parametros da axis-aligned bounding box (AABB) do modelo.
    glm::vec3 bbox_min = g_VirtualScene[object_name].bbox_min;
    glm::vec3 bbox_max = g_VirtualScene[object_name].bbox_max;
    glUniform4f(bbox_min_uniform, bbox_min.x, bbox_min.y, bbox_min.z, 1.0f);
    glUniform4f(bbox_max_uniform, bbox_max.x, bbox_max.y, bbox_max.z, 1.0f);

    // Pedimos para a GPU rasterizar os vertices dos eixos XYZ
    // apontados pelo VAO como linhas. Veja a definicao de
    // g_VirtualScene[""] dentro da funcao BuildTrianglesAndAddToVirtualScene(), e veja
    // a documentacao da funcao glDrawElements() em
    // http://docs.gl/gl3/glDrawElements.
    glDrawElements(
        g_VirtualScene[object_name].rendering_mode,
        g_VirtualScene[object_name].num_indices,
        GL_UNSIGNED_INT,
        (void*)g_VirtualScene[object_name].first_index
    );

    // "Desligamos" o VAO, evitando assim que operacoes posteriores venham a
    // alterar o mesmo. Isso evita bugs.
    glBindVertexArray(0);
}

// Funcao que carrega os shaders de vertices e de fragmentos que serao
// utilizados para renderizacao. Veja slide 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
//
void LoadShadersFromFiles()
{
    // Note que o caminho para os arquivos "shader_vertex.glsl" e
    // "shader_fragment.glsl" est�o fixados, sendo que assumimos a exist�ncia
    // da seguinte estrutura no sistema de arquivos:
    //
    //    + FCG_Lab_01/
    //    |
    //    +--+ bin/
    //    |  |
    //    |  +--+ Release/  (ou Debug/ ou Linux/)
    //    |     |
    //    |     o-- main.exe
    //    |
    //    +--+ src/
    //       |
    //       o-- shader_vertex.glsl
    //       |
    //       o-- shader_fragment.glsl
    //
    vertex_shader_id = LoadShader_Vertex("../../src/shader_vertex.glsl");
    fragment_shader_id = LoadShader_Fragment("../../src/shader_fragment.glsl");

    // Deletamos o programa de GPU anterior, caso ele exista.
    if ( program_id != 0 )
        glDeleteProgram(program_id);

    // Criamos um programa de GPU utilizando os shaders carregados acima.
    program_id = CreateGpuProgram(vertex_shader_id, fragment_shader_id);

    // Buscamos o endere�o das vari�veis definidas dentro do Vertex Shader.
    // Utilizaremos estas vari�veis para enviar dados para a placa de video
    // (GPU)! Veja arquivo "shader_vertex.glsl" e "shader_fragment.glsl".
    model_uniform           = glGetUniformLocation(program_id, "model"); // Variavel da matriz "model"
    view_uniform            = glGetUniformLocation(program_id, "view"); // Variavel da matriz "view" em shader_vertex.glsl
    projection_uniform      = glGetUniformLocation(program_id, "projection"); // Variavel da matriz "projection" em shader_vertex.glsl
    object_id_uniform       = glGetUniformLocation(program_id, "object_id"); // Variavel "object_id" em shader_fragment.glsl
    plane_type_uniform      = glGetUniformLocation(program_id, "plane_type");
    bbox_min_uniform        = glGetUniformLocation(program_id, "bbox_min");
    bbox_max_uniform        = glGetUniformLocation(program_id, "bbox_max");

    // Vari�veis em "shader_fragment.glsl" para acesso das imagens de textura
    glUseProgram(program_id);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage0"), 0);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage1"), 1);
    glUniform1i(glGetUniformLocation(program_id, "TextureImage2"), 2);
    glUseProgram(0);
}

// Funcao que pega a matriz M e guarda a mesma no topo da pilha
void PushMatrix(glm::mat4 M)
{
    g_MatrixStack.push(M);
}

// Funcao que remove a matriz atualmente no topo da pilha e armazena a mesma na variavel M
void PopMatrix(glm::mat4& M)
{
    if ( g_MatrixStack.empty() )
    {
        M = Matrix_Identity();
    }
    else
    {
        M = g_MatrixStack.top();
        g_MatrixStack.pop();
    }
}

// Funcao que computa as normais de um ObjModel, caso elas nao tenham sido
// especificadas dentro do arquivo ".obj"
void ComputeNormals(ObjModel* model)
{
    if ( !model->attrib.normals.empty() )
        return;

    // Primeiro computamos as normais para todos os TRIANGULOS.
    // Segundo, computamos as normais dos VERTICES atraves do metodo proposto
    // por Gouraud, onde a normal de cada vertice vai ser a media das normais de
    // todas as faces que compartilham este vertice.

    size_t num_vertices = model->attrib.vertices.size() / 3;

    std::vector<int> num_triangles_per_vertex(num_vertices, 0);
    std::vector<glm::vec4> vertex_normals(num_vertices, glm::vec4(0.0f,0.0f,0.0f,0.0f));

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            glm::vec4  vertices[3];
            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                vertices[vertex] = glm::vec4(vx,vy,vz,1.0);
            }

            const glm::vec4  a = vertices[0];
            const glm::vec4  b = vertices[1];
            const glm::vec4  c = vertices[2];

            const glm::vec4  n = crossproduct(b-a,c-a);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];
                num_triangles_per_vertex[idx.vertex_index] += 1;
                vertex_normals[idx.vertex_index] += n;
                model->shapes[shape].mesh.indices[3*triangle + vertex].normal_index = idx.vertex_index;
            }
        }
    }

    model->attrib.normals.resize( 3*num_vertices );

    for (size_t i = 0; i < vertex_normals.size(); ++i)
    {
        glm::vec4 n = vertex_normals[i] / (float)num_triangles_per_vertex[i];
        n /= norm(n);
        model->attrib.normals[3*i + 0] = n.x;
        model->attrib.normals[3*i + 1] = n.y;
        model->attrib.normals[3*i + 2] = n.z;
    }
}

// Constroi triangulos para futura renderizacao a partir de um ObjModel.
void BuildTrianglesAndAddToVirtualScene(ObjModel* model)
{
    GLuint vertex_array_object_id;
    glGenVertexArrays(1, &vertex_array_object_id);
    glBindVertexArray(vertex_array_object_id);

    std::vector<GLuint> indices;
    std::vector<float>  model_coefficients;
    std::vector<float>  normal_coefficients;
    std::vector<float>  texture_coefficients;

    for (size_t shape = 0; shape < model->shapes.size(); ++shape)
    {
        size_t first_index = indices.size();
        size_t num_triangles = model->shapes[shape].mesh.num_face_vertices.size();

        const float minval = std::numeric_limits<float>::min();
        const float maxval = std::numeric_limits<float>::max();

        glm::vec3 bbox_min = glm::vec3(maxval,maxval,maxval);
        glm::vec3 bbox_max = glm::vec3(minval,minval,minval);

        for (size_t triangle = 0; triangle < num_triangles; ++triangle)
        {
            assert(model->shapes[shape].mesh.num_face_vertices[triangle] == 3);

            for (size_t vertex = 0; vertex < 3; ++vertex)
            {
                tinyobj::index_t idx = model->shapes[shape].mesh.indices[3*triangle + vertex];

                indices.push_back(first_index + 3*triangle + vertex);

                const float vx = model->attrib.vertices[3*idx.vertex_index + 0];
                const float vy = model->attrib.vertices[3*idx.vertex_index + 1];
                const float vz = model->attrib.vertices[3*idx.vertex_index + 2];
                model_coefficients.push_back( vx ); // X
                model_coefficients.push_back( vy ); // Y
                model_coefficients.push_back( vz ); // Z
                model_coefficients.push_back( 1.0f ); // W

                bbox_min.x = std::min(bbox_min.x, vx);
                bbox_min.y = std::min(bbox_min.y, vy);
                bbox_min.z = std::min(bbox_min.z, vz);
                bbox_max.x = std::max(bbox_max.x, vx);
                bbox_max.y = std::max(bbox_max.y, vy);
                bbox_max.z = std::max(bbox_max.z, vz);

                // Inspecionando o c�digo da tinyobjloader, o aluno Bernardo
                // Sulzbach (2017/1) apontou que a maneira correta de testar se
                // existem normais e coordenadas de textura no ObjModel �
                // comparando se o �ndice retornado � -1. Fazemos isso abaixo.

                if ( idx.normal_index != -1 )
                {
                    const float nx = model->attrib.normals[3*idx.normal_index + 0];
                    const float ny = model->attrib.normals[3*idx.normal_index + 1];
                    const float nz = model->attrib.normals[3*idx.normal_index + 2];
                    normal_coefficients.push_back( nx ); // X
                    normal_coefficients.push_back( ny ); // Y
                    normal_coefficients.push_back( nz ); // Z
                    normal_coefficients.push_back( 0.0f ); // W
                }

                if ( idx.texcoord_index != -1 )
                {
                    const float u = model->attrib.texcoords[2*idx.texcoord_index + 0];
                    const float v = model->attrib.texcoords[2*idx.texcoord_index + 1];
                    texture_coefficients.push_back( u );
                    texture_coefficients.push_back( v );
                }
            }
        }

        size_t last_index = indices.size() - 1;

        SceneObject theobject;
        theobject.name           = model->shapes[shape].name;
        theobject.first_index    = (void*)first_index; // Primeiro indice
        theobject.num_indices    = last_index - first_index + 1; // Numero de indices
        theobject.rendering_mode = GL_TRIANGLES;       // indices correspondem ao tipo de rasterizacao GL_TRIANGLES.
        theobject.vertex_array_object_id = vertex_array_object_id;

        theobject.bbox_min = bbox_min;
        theobject.bbox_max = bbox_max;

        g_VirtualScene[model->shapes[shape].name] = theobject;
    }

    GLuint VBO_model_coefficients_id;
    glGenBuffers(1, &VBO_model_coefficients_id);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_model_coefficients_id);
    glBufferData(GL_ARRAY_BUFFER, model_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, model_coefficients.size() * sizeof(float), model_coefficients.data());
    GLuint location = 0; // "(location = 0)" em "shader_vertex.glsl"
    GLint  number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
    glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(location);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if ( !normal_coefficients.empty() )
    {
        GLuint VBO_normal_coefficients_id;
        glGenBuffers(1, &VBO_normal_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_normal_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, normal_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, normal_coefficients.size() * sizeof(float), normal_coefficients.data());
        location = 1; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 4; // vec4 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if ( !texture_coefficients.empty() )
    {
        GLuint VBO_texture_coefficients_id;
        glGenBuffers(1, &VBO_texture_coefficients_id);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_texture_coefficients_id);
        glBufferData(GL_ARRAY_BUFFER, texture_coefficients.size() * sizeof(float), NULL, GL_STATIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, texture_coefficients.size() * sizeof(float), texture_coefficients.data());
        location = 2; // "(location = 1)" em "shader_vertex.glsl"
        number_of_dimensions = 2; // vec2 em "shader_vertex.glsl"
        glVertexAttribPointer(location, number_of_dimensions, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(location);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    GLuint indices_id;
    glGenBuffers(1, &indices_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(GLuint), indices.data());

    glBindVertexArray(0);
}

// Carrega um Vertex Shader de um arquivo GLSL. Veja definicao de LoadShader() abaixo.
GLuint LoadShader_Vertex(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // sera aplicado nos vertices.
    GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, vertex_shader_id);

    // Retorna o ID gerado acima
    return vertex_shader_id;
}

// Carrega um Fragment Shader de um arquivo GLSL . Veja definicao de LoadShader() abaixo.
GLuint LoadShader_Fragment(const char* filename)
{
    // Criamos um identificador (ID) para este shader, informando que o mesmo
    // sera aplicado nos fragmentos.
    GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

    // Carregamos e compilamos o shader
    LoadShader(filename, fragment_shader_id);

    // Retorna o ID gerado acima
    return fragment_shader_id;
}

// Funcao auxilar, utilizada pelas duas funcoes acima. Carrega codigo de GPU de
// um arquivo GLSL e faz sua compilacao.
void LoadShader(const char* filename, GLuint shader_id)
{
    // Lemos o arquivo de texto indicado pela vari�vel "filename"
    // e colocamos seu conteudo em memoria, apontado pela variavel
    // "shader_string".
    std::ifstream file;
    try {
        file.exceptions(std::ifstream::failbit);
        file.open(filename);
    } catch ( std::exception& e ) {
        fprintf(stderr, "ERROR: Cannot open file \"%s\".\n", filename);
        std::exit(EXIT_FAILURE);
    }
    std::stringstream shader;
    shader << file.rdbuf();
    std::string str = shader.str();
    const GLchar* shader_string = str.c_str();
    const GLint   shader_string_length = static_cast<GLint>( str.length() );

    // Define o codigo do shader GLSL, contido na string "shader_string"
    glShaderSource(shader_id, 1, &shader_string, &shader_string_length);

    // Compila o codigo do shader GLSL (em tempo de execucao)
    glCompileShader(shader_id);

    // Verificamos se ocorreu algum erro ou "warning" durante a compilacao
    GLint compiled_ok;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compiled_ok);

    GLint log_length = 0;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    // Alocamos memoria para guardar o log de compilacao.
    // A chamada "new" em C++ e equivalente ao "malloc()" do C.
    GLchar* log = new GLchar[log_length];
    glGetShaderInfoLog(shader_id, log_length, &log_length, log);

    // Imprime no terminal qualquer erro ou "warning" de compilacao
    if ( log_length != 0 )
    {
        std::string  output;

        if ( !compiled_ok )
        {
            output += "ERROR: OpenGL compilation of \"";
            output += filename;
            output += "\" failed.\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }
        else
        {
            output += "WARNING: OpenGL compilation of \"";
            output += filename;
            output += "\".\n";
            output += "== Start of compilation log\n";
            output += log;
            output += "== End of compilation log\n";
        }

        fprintf(stderr, "%s", output.c_str());
    }

    // A chamada "delete" em C++ e equivalente ao "free()" do C
    delete [] log;
}

// Esta funcao cria um programa de GPU, o qual contem obrigatoriamente um
// Vertex Shader e um Fragment Shader.
GLuint CreateGpuProgram(GLuint vertex_shader_id, GLuint fragment_shader_id)
{
    // Criamos um identificador (ID) para este programa de GPU
    GLuint program_id = glCreateProgram();

    // Definicao dos dois shaders GLSL que devem ser executados pelo programa
    glAttachShader(program_id, vertex_shader_id);
    glAttachShader(program_id, fragment_shader_id);

    // Linkagem dos shaders acima ao programa
    glLinkProgram(program_id);

    // Verificamos se ocorreu algum erro durante a linkagem
    GLint linked_ok = GL_FALSE;
    glGetProgramiv(program_id, GL_LINK_STATUS, &linked_ok);

    // Imprime no terminal qualquer erro de linkagem
    if ( linked_ok == GL_FALSE )
    {
        GLint log_length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);

        // Alocamos memoria para guardar o log de compilacao.
        // A chamada "new" em C++ e equivalente ao "malloc()" do C.
        GLchar* log = new GLchar[log_length];

        glGetProgramInfoLog(program_id, log_length, &log_length, log);

        std::string output;

        output += "ERROR: OpenGL linking of program failed.\n";
        output += "== Start of link log\n";
        output += log;
        output += "\n== End of link log\n";

        // A chamada "delete" em C++ e equivalente ao "free()" do C
        delete [] log;

        fprintf(stderr, "%s", output.c_str());
    }

    // Os "Shader Objects" podem ser marcados para delecao apos serem linkados
    glDeleteShader(vertex_shader_id);
    glDeleteShader(fragment_shader_id);

    // Retornamos o ID gerado acima
    return program_id;
}

// Definicao da funcao que sera chamada sempre que a janela do sistema
// operacional for redimensionada, por consequencia alterando o tamanho do
// "framebuffer" (regiao de memoria onde sao armazenados os pixels da imagem).
void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    // Indicamos que queremos renderizar em toda regiao do framebuffer. A
    // funçao "glViewport" define o mapeamento das "normalized device
    // coordinates" (NDC) para "pixel coordinates".  Essa e a operacao de
    // "Screen Mapping" ou "Viewport Mapping" vista em aula
    glViewport(0, 0, width, height);

    // Atualizamos tambem a razao que define a proporcao da janela (largura /
    // altura), a qual sera utilizada na definicao das matrizes de projecao,
    // tal que nao ocorra distorcoes durante o processo de "Screen Mapping"
    // acima, quando NDC e mapeado para coordenadas de pixels. Veja slide 205-215 do documento Aula_09_Projecoes.pdf.
    //
    // O cast para float e necessario pois numeros inteiros sao arredondados ao
    // serem divididos!
    g_ScreenRatio = (float)width / height;
}

// Variaveis globais que armazenam a ultima posicao do cursor do mouse, para
// que possamos calcular quanto que o mouse se movimentou entre dois instantes
// de tempo. Utilizadas no callback CursorPosCallback() abaixo.
double g_LastCursorPosX, g_LastCursorPosY;

// Funcao callback chamada sempre que o usuario aperta algum dos botoes do mouse
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // Se o usuario pressionou o botao esquerdo do mouse, guardamos a
        // posicao atual do cursor nas variaveis g_LastCursorPosX e
        // g_LastCursorPosY.  Tambem, setamos a variavel
        // g_LeftMouseButtonPressed como true, para saber que o usuario esta
        // com o botao esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_LeftMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        // Quando o usuario soltar o botao esquerdo do mouse, atualizamos a
        // variavel abaixo para false.
        g_LeftMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        // Se o usuario pressionou o botao esquerdo do mouse, guardamos a
        // posicao atual do cursor nas variaveis g_LastCursorPosX e
        // g_LastCursorPosY.  Tambem, setamos a variavel
        // g_RightMouseButtonPressed como true, para saber que o usuario esta
        // com o botao esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_RightMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        // Quando o usuario soltar o botao esquerdo do mouse, atualizamos a
        // variavel abaixo para false.
        g_RightMouseButtonPressed = false;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        // Se o usuario pressionou o botao esquerdo do mouse, guardamos a
        // posicao atual do cursor nas variaveis g_LastCursorPosX e
        // g_LastCursorPosY.  Tambem, setamos a variavel
        // g_MiddleMouseButtonPressed como true, para saber que o usuario esta
        // com o botao esquerdo pressionado.
        glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
        g_MiddleMouseButtonPressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        // Quando o usuario soltar o botao esquerdo do mouse, atualizamos a
        // variavel abaixo para false.
        g_MiddleMouseButtonPressed = false;
    }
}

bool cursorPosCallbackHasExecuted;

// Funcao callback chamada sempre que o usuario movimentar o cursor do mouse em
// cima da janela OpenGL.
void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    // Abaixo executamos o seguinte: caso o botao esquerdo do mouse esteja
    // pressionado, computamos quanto que o mouse se movimento desde o ultimo
    // instante de tempo, e usamos esta movimentacao para atualizar os
    // parametros que definem a posicao da camera dentro da cena virtual.
    // Assim, temos que o usuario consegue controlar a camera.

        if(cursorPosCallbackHasExecuted){
          // Deslocamento do cursor do mouse em x e y de coordenadas de tela!
          float dx = xpos - g_LastCursorPosX;
          float dy = ypos - g_LastCursorPosY;

          // Atualizamos parametros da camera com os deslocamentos
          g_CameraTheta -= 0.01f*dx;
          g_CameraPhi   -= 0.01f*dy;

          // Em coordenadas esfericas, o angulo phi deve ficar entre -pi/2 e +pi/2.
          float phimax = 3.141592f/2;
          float phimin = -phimax;

          if (g_CameraPhi > phimax)
              g_CameraPhi = phimax;

          if (g_CameraPhi < phimin)
              g_CameraPhi = phimin;
        }
        // Atualizamos as variaveis globais para armazenar a posicao atual do
        // cursor como sendo a ultima posicao conhecida do cursor.
        g_LastCursorPosX = xpos;
        g_LastCursorPosY = ypos;

        cursorPosCallbackHasExecuted = true;
}


// Definicao da funcao que sera chamada sempre que o usuario pressionar alguma
// tecla do teclado. Veja http://www.glfw.org/docs/latest/input_guide.html#input_key
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mod)
{
    // Se o usuario pressionar a tecla ESC, fechamos a janela.
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // Se o usuario aperta espaco, atira
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
    {
      if (key_space_pressed == false) {
        key_space_pressed = true;
      }
    }

    // Se o usuario apertar a tecla P, trocamos para projecao perspectiva.
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = true;
    }

    // Se o usuario apertar a tecla O, trocamos para projecao ortografica.
    if (key == GLFW_KEY_O && action == GLFW_PRESS)
    {
        g_UsePerspectiveProjection = false;
    }

    // Testar se W, A, S, D foram pressionadas
    if (key == GLFW_KEY_W && action == GLFW_PRESS)
    {
        key_w_pressed = true;
    }

    if  (key == GLFW_KEY_A && action == GLFW_PRESS)
    {
        key_a_pressed = true;
    }

     if (key == GLFW_KEY_S && action == GLFW_PRESS)
    {
        key_s_pressed = true;
    }

    if (key == GLFW_KEY_D && action == GLFW_PRESS)
    {
        key_d_pressed = true;
    }

    // Testar se W, A, S, D foram soltas
    if (key == GLFW_KEY_W && action == GLFW_RELEASE)
    {
        key_w_pressed = false;
    }

    if (key == GLFW_KEY_S && action == GLFW_RELEASE)
    {
        key_s_pressed = false;
    }

    if (key == GLFW_KEY_A && action == GLFW_RELEASE)
    {
        key_a_pressed = false;
    }

    if (key == GLFW_KEY_D && action == GLFW_RELEASE)
    {
        key_d_pressed = false;
    }
}

// Definimos o callback para impressao de erros da GLFW no terminal
void ErrorCallback(int error, const char* description)
{
    fprintf(stderr, "ERROR: GLFW: %s\n", description);
}

#define MOV_SPEED 5.0

glm::vec4 scale(glm::vec4 v, float s){
    float n = norm(v);
    float u1 = v.x / n;
    float u2 = v.y / n;
    float u3 = v.z / n;
    float u4 = v.w / n;
    return glm::vec4(u1*s, u2*s, u3*s, u4*s);
}

double prevTime = -1.0f;

void updateCameraPosition(glm::vec4 &camera_view_vector){

    double currentTime = glfwGetTime();
    glm::vec4 newCameraPositionZ, newCameraPositionX, newCameraPosition;

    if(prevTime > 0){
      glm::vec4 rotated_vector = crossproduct(camera_view_vector, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
      glm::vec4 front_vector = crossproduct(rotated_vector, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));

      glm::vec4 z_componentfv = scale(glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), dotproduct(front_vector, glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
      glm::vec4 x_componentfv = scale(glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), dotproduct(front_vector, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)));
      glm::vec4 z_componentrv = scale(glm::vec4(0.0f, 0.0f, 1.0f, 0.0f), dotproduct(rotated_vector, glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
      glm::vec4 x_componentrv = scale(glm::vec4(1.0f, 0.0f, 0.0f, 0.0f), dotproduct(rotated_vector, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)));

      float speed = MOV_SPEED;
      speed *= (currentTime - prevTime);

      if(key_w_pressed){
        newCameraPosition = camera_movement - scale(front_vector, speed);
      }

      if(key_s_pressed){
        newCameraPosition = camera_movement + scale(front_vector, speed);
      }

      if(key_a_pressed){
        newCameraPosition = camera_movement - scale(rotated_vector, speed);
      }

      if(key_d_pressed){
        newCameraPosition = camera_movement + scale(rotated_vector, speed);
      }

      bool shouldUpdate = true;

      shouldUpdate &= (!CheckWallCollision(newCameraPosition, sceneWalls[0]));;
      shouldUpdate &= (!CheckWallCollision(newCameraPosition, sceneWalls[1]));;
      shouldUpdate &= (!CheckWallYZCollision(newCameraPosition, sceneWalls[2]));;
      shouldUpdate &= (!CheckWallYZCollision(newCameraPosition, sceneWalls[3]));;

      if(shouldUpdate){
        if(key_w_pressed){
          camera_movement = camera_movement - scale(front_vector, speed);
        }

        if(key_s_pressed){
          camera_movement = camera_movement + scale(front_vector, speed);
        }

        if(key_a_pressed){
          camera_movement = camera_movement - scale(rotated_vector, speed);
        }

        if(key_d_pressed){
          camera_movement = camera_movement + scale(rotated_vector, speed);
        }
      } else{

        if(key_w_pressed){
          newCameraPositionZ = camera_movement - scale(z_componentfv, speed);
          newCameraPositionX = camera_movement - scale(x_componentfv, speed);
        }

        if(key_s_pressed){
          newCameraPositionZ = camera_movement + scale(z_componentfv, speed);
          newCameraPositionX = camera_movement + scale(x_componentfv, speed);
        }

        if(key_a_pressed){
          newCameraPositionZ = camera_movement - scale(z_componentrv, speed);
          newCameraPositionX = camera_movement - scale(x_componentrv, speed);
        }

        if(key_d_pressed){
          newCameraPositionZ = camera_movement + scale(z_componentrv, speed);
          newCameraPositionX = camera_movement + scale(x_componentrv, speed);
        }

        shouldUpdate = true;

        // checa se a camera colide com a parede
        shouldUpdate &= (!CheckWallCollision(newCameraPositionZ, sceneWalls[0]));
        shouldUpdate &= (!CheckWallCollision(newCameraPositionZ, sceneWalls[1]));

        if(shouldUpdate){
          if(key_w_pressed){
              camera_movement -= scale(z_componentfv, speed);
          }

          if(key_s_pressed){
              camera_movement += scale(z_componentfv, speed);
          }

          if(key_a_pressed){
              camera_movement -= scale(z_componentrv, speed);
          }

          if(key_d_pressed){
              camera_movement += scale(z_componentrv, speed);
          }
        }

        shouldUpdate = true;

        shouldUpdate &= (!CheckWallYZCollision(newCameraPositionX, sceneWalls[2]));
        shouldUpdate &= (!CheckWallYZCollision(newCameraPositionX, sceneWalls[3]));

        if(shouldUpdate){
          if(key_w_pressed){
             camera_movement -= scale(x_componentfv, speed);
          }

          if(key_s_pressed){
             camera_movement += scale(x_componentfv, speed);
          }

          if(key_a_pressed){
             camera_movement -= scale(x_componentrv, speed);
          }

          if(key_d_pressed){
             camera_movement += scale(x_componentrv, speed);
          }
        }
      }
    }

    prevTime = currentTime;
}

RoomWallModel CreateWallX(float positionX, float positionY, float positionZ, float scaleX, float scaleZ, int objType)
{
    glm::mat4 model = Matrix_Identity(); // Transformacao identidade de modelagem
    float defaultScaleY = 1.0f;

    model = Matrix_Translate(positionX, positionY, positionZ);
    model *= Matrix_Rotate_Z(0.0) * Matrix_Rotate_Y(0.0) * Matrix_Rotate_X(PI / 2.0);
    model *= Matrix_Scale(scaleX, defaultScaleY, scaleZ);
    glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
    glUniform1i(object_id_uniform, PLANE);
    glUniform1i(plane_type_uniform, objType);
    DrawVirtualObject("plane");

    RoomWallModel returnModel;
    returnModel.positionX = positionX; returnModel.positionY = positionY; returnModel.positionZ = positionZ;
    returnModel.scaleX = scaleX; returnModel.scaleY = defaultScaleY; returnModel.scaleZ = scaleZ;

    return returnModel;
}

RoomWallModel CreateWallY(float positionX, float positionY, float positionZ, float scaleX, float scaleZ, int objType)
{
    glm::mat4 model = Matrix_Identity(); // Transformacao identidade de modelagem
    float defaultScaleY = 1.0f;

    model = Matrix_Translate(positionX, positionY, positionZ);
    model *= Matrix_Rotate_Y(PI / 2.0) * Matrix_Rotate_X(PI / 2.0);
    model *= Matrix_Scale(scaleX, defaultScaleY, scaleZ);
    glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
    glUniform1i(object_id_uniform, PLANE);
    glUniform1i(plane_type_uniform, objType);
    DrawVirtualObject("plane");

    RoomWallModel returnModel;
    returnModel.positionX = positionX; returnModel.positionY = positionY; returnModel.positionZ = positionZ;
    returnModel.scaleX = scaleX; returnModel.scaleY = defaultScaleY; returnModel.scaleZ = scaleZ;

    return returnModel;
}

RoomWallModel CreateFloor(float positionX, float positionY, float positionZ, float scaleX, float scaleZ, int objType)
{
    glm::mat4 model = Matrix_Identity(); // Transformacao identidade de modelagem
    float defaultScaleY = 1.0f;

    model = Matrix_Translate(positionX, positionY, positionZ);
    model *= Matrix_Scale(scaleX, defaultScaleY,scaleZ);
    glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
    glUniform1i(object_id_uniform, PLANE);
    glUniform1i(plane_type_uniform, objType);
    DrawVirtualObject("plane");

    RoomWallModel returnModel;
    returnModel.positionX = positionX; returnModel.positionY = positionY; returnModel.positionZ = positionZ;
    returnModel.scaleX = scaleX; returnModel.scaleY = defaultScaleY; returnModel.scaleZ = scaleZ;

    return returnModel;
}

RoomWallModel MakeGetObj(float positionX, float positionY, float positionZ)
{
    RoomWallModel model;
    model.positionX = positionX;
    model.positionY = positionY;
    model.positionZ = positionZ;
    model.scaleX = 0.2f;
    model.scaleY = 0.2f;
    model.scaleZ = 0.2f;
    return model;
}

#define CUBE_SPEED 20.0

void UpdateGetObj(RoomWallModel &shot, float timeElapsed)
{
    if(timeElapsed > 0)
    {
        shot.positionZ -= CUBE_SPEED * timeElapsed;
    }
}

void DrawGetObj(RoomWallModel shot)
{
    glm::mat4 model =
      Matrix_Translate(shot.positionX, shot.positionY, shot.positionZ)
    * Matrix_Scale(shot.scaleX, shot.scaleY, shot.scaleZ);
    glUniformMatrix4fv(model_uniform, 1 , GL_FALSE , glm::value_ptr(model));
    glUniform1i(object_id_uniform, GET_OBJ);
    DrawVirtualObject("cube");
}

glm::vec4 TextMessage(char* message)
{
    double timeToWait = glfwGetTime();

    while(glfwGetTime() - timeToWait < 3)
    {
        char buffer[80];
        float padding = TextRendering_LineHeight(window);
        snprintf(buffer, 80, message);
        TextRendering_PrintString(window, buffer, -0.7 + padding/3, 0.4f + 2*padding/5, 1.0f);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    return glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
}


