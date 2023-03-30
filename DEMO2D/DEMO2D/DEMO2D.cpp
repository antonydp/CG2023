// 2D_SEA.cpp : Questo file contiene la funzione 'main', in cui inizia e termina l'esecuzione del programma.
//
// Con il tasto ‘r’/'R' il cannone ruota destra/sinistra
// Con il tasto ‘b’ viene visualizzato il bounding box della farfalla.
// con la barra spaziatrice il cannone spara un proiettile, 

// TASK: l’obiettivo è distruggere la farfalla. 
// Quando la farfalla viene colpita non viene più disegnata.

//NOTA La farfalla è disegnata con la curva parametrica che la descrive, 
// il cannone è fatto con le primitive di base.

#include <iostream>
#include "ShaderMaker.h"
#include <GL/glew.h>
#include <GL/freeglut.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <vector>
#include <functional>

using namespace glm;
using namespace std;

static unsigned int programId, programId_text;

#define  PI   3.14159265358979323846

mat4 Projection;
GLuint MatProj, MatModel, loctime, locres;
int nv_P;

// viewport size
int width = 1000;
int height = 720;


//STRUTTURA FIGURA
typedef struct {
	GLuint VAO;
	GLuint VBO_G;
	GLuint VBO_C;
	int nTriangles;
	// Vertici
	vector<vec3> vertici;
	vector<vec4> colors;
	// Numero vertici
	int nv;
	//Matrice di Modellazione: Traslazione*Rotazione*Scala
	mat4 Model;
	vec4 corner_b_obj;
	vec4 corner_t_obj;
	vec4 corner_b;
	vec4 corner_t;
	bool alive;
} Figura;

Figura Esempio;
//SCENA
vector<Figura> Scena;

void crea_VAO_Vector(Figura* fig)
{
	glGenVertexArrays(1, &fig->VAO);
	glBindVertexArray(fig->VAO);
	//Genero , rendo attivo, riempio il VBO della geometria dei vertici
	glGenBuffers(1, &fig->VBO_G);
	glBindBuffer(GL_ARRAY_BUFFER, fig->VBO_G);
	glBufferData(GL_ARRAY_BUFFER, fig->vertici.size() * sizeof(vec3), fig->vertici.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(0);

	//Genero , rendo attivo, riempio il VBO dei colori
	glGenBuffers(1, &fig->VBO_C);
	glBindBuffer(GL_ARRAY_BUFFER, fig->VBO_C);
	glBufferData(GL_ARRAY_BUFFER, fig->colors.size() * sizeof(vec4), fig->colors.data(), GL_STATIC_DRAW);
	//Adesso carico il VBO dei colori nel layer 2
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(1);

}

void costruisci_figura_parametrica(int nTriangles, std::function<float(float)> fx, std::function<float(float)> fy, vec4 color, Figura* fig) {

	int i;
	float stepA = (2 * PI) / fig->nTriangles;
	float t;
	 
	for (i = 0; i <= fig->nTriangles; i++)
	{
		t = (float)i * stepA;
		fig->vertici.push_back(vec3(fx(t), fy(t), 0.0));
		//Colore 
		fig->colors.push_back(color);
	}

	fig->nv = fig->vertici.size();

}

void init_shader(void)
{
	GLenum ErrorCheckValue = glGetError();

	char* vertexShader = (char*)"vertexShader_M.glsl";
	char* fragmentShader = (char*)"FS_mare.glsl";
	programId = ShaderMaker::createProgram(vertexShader, fragmentShader);
	glUseProgram(programId);
}

void init(void)
{
	Esempio.nTriangles = 180;
	costruisci_figura_parametrica(,&Esempio);
	crea_VAO_Vector(&Esempio);
	Esempio.alive = TRUE;
	Scena.push_back(Esempio);


 
	MatProj = glGetUniformLocation(programId, "Projection");
	MatModel = glGetUniformLocation(programId, "Model");
	loctime = glGetUniformLocation(programId, "time");
	locres = glGetUniformLocation(programId, "resolution");
}

double  degtorad(double angle) {
	return angle * PI / 180;
} 
void drawScene(void)
{
	int i = 0, j = 0;
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glPointSize(14.0);
	float time = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;

	vec2 resolution = vec2(w_update, h_update);

	glUniform1f(loctime, time);
	glUniform2f(locres, resolution.x, resolution.y);
	glUniformMatrix4fv(MatProj, 1, GL_FALSE, value_ptr(Projection));

	//Disegna Cielo e Mare
	for (i = Scena.size() - 2; i < Scena.size(); i++)
	{
		glUniformMatrix4fv(MatModel, 1, GL_FALSE, value_ptr(Scena[i].Model));
		glBindVertexArray(Scena[i].VAO);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, Scena[i].nv);
		glBindVertexArray(0);
	}
	/*Matrice di modellazione del cannone */
	Scena[1].Model = mat4(1.0);
	Scena[1].Model = translate(Scena[1].Model, vec3(600.0 + dx, 300.0 + dy, 0.0));
	Scena[1].Model = rotate(Scena[1].Model, angoloCannone, vec3(0.0f, 0.0f, 1.0f));
	Scena[1].Model = rotate(Scena[1].Model, angolo, vec3(0.0f, 0.0f, 1.0f));

	/*Matrice di modellazione del proiettile*/
	Scena[2].Model = translate(Scena[1].Model, vec3(posx_Proiettile, 80 + posy_Proiettile, 0.0));
	Scena[2].Model = scale(Scena[2].Model, vec3(30.5, 30.5, 1.0));
	Scena[2].corner_b = Scena[2].corner_b_obj;
	Scena[2].corner_t = Scena[2].corner_t_obj;
	Scena[2].corner_b = Scena[2].Model * Scena[2].corner_b;
	Scena[2].corner_t = Scena[2].Model * Scena[2].corner_t;
	/*Update Scala Matrice di modellazione del cannone */
	Scena[1].Model = scale(Scena[1].Model, vec3(60.0, 60.5, 1.0));

	//Disegno Proiettile
	glUniformMatrix4fv(MatModel, 1, GL_FALSE, value_ptr(Scena[2].Model));
	glBindVertexArray(Scena[2].VAO);
	glDrawArrays(GL_TRIANGLE_FAN, 0, Scena[2].nv - 6);

	if (drawBB == TRUE)
	{
		//Disegno Bounding Box
		glDrawArrays(GL_LINE_STRIP, Scena[2].nv - 6, 6);
		glBindVertexArray(0);
	}

	//Disegno Cannone
	glUniformMatrix4fv(MatModel, 1, GL_FALSE, value_ptr(Scena[1].Model));
	glBindVertexArray(Scena[1].VAO);
	glDrawArrays(GL_TRIANGLE_FAN, 0, Scena[1].nv - 12);
	glDrawArrays(GL_TRIANGLES, Scena[1].nv - 12, 12);
	glBindVertexArray(0);

	/*Matrice di modellazione farfalla */
	Scena[0].Model = mat4(1.0);
	Scena[0].Model = translate(Scena[0].Model, vec3(900.0 - dx_f, 500.0 - dy_f, 0.0));
	Scena[0].Model = scale(Scena[0].Model, vec3(80.5, 80.5, 1.0));

	Scena[0].corner_b = Scena[0].corner_b_obj;
	Scena[0].corner_t = Scena[0].corner_t_obj;
	//printf("Farfalla \n");
	Scena[0].corner_b = Scena[0].Model * Scena[0].corner_b;
	//std::cout << glm::to_string(Scena[0].corner_b) << std::endl;
	Scena[0].corner_t = Scena[0].Model * Scena[0].corner_t;
	//std::cout << glm::to_string(Scena[0].corner_t) << std::endl;
	 

	if (Coll == TRUE)
		Scena[0].alive = FALSE;

	//Disegno la farfalla fino a quando non sia avvenuta la prima collisione con la palla del cannone
	if (Scena[0].alive == TRUE)
	{
		glUniformMatrix4fv(MatModel, 1, GL_FALSE, value_ptr(Scena[0].Model));
		glBindVertexArray(Scena[0].VAO);
		glDrawArrays(GL_TRIANGLE_FAN, 0, Scena[0].nv - 6);
		if (drawBB == TRUE)
			//Disegno Bounding Box
			glDrawArrays(GL_LINE_STRIP, Scena[0].nv - 6, 6);
		glBindVertexArray(0);
	}
	glutSwapBuffers();
}

void keyboardReleasedEvent(unsigned char key, int x, int y)
{
	{
		switch (key)
		{
		case 'b':
			drawBB = FALSE;
			break;
		default:
			break;
		}
	}
	glutPostRedisplay();
}
void myKeyboard(unsigned char key, int x, int y)
{
	{
		switch (key)
		{
		case ' ':
			updateProiettile(0);
			break;

		case 'a':
			dx -= 1;
			break;

		case 'd':
			dx += 1;
			break;

		case 'R':
			angolo += 0.1;
			break;
		case 'r':
			angolo -= 0.1;
			break;

		case 't':
			angolo += 0.1;
			break;

		case 'b':
			drawBB = TRUE;
			break;
		default:
			break;
		}
	}
	glutPostRedisplay();
}
void update_f(int value)
{
	dx_f = dx_f + 0.1;
	dy_f = dy_f - 0.1;
	glutTimerFunc(25, update_f, 0);
	glutPostRedisplay();
}
void reshape(int w, int h)
{
	Projection = ortho(0.0f, (float)width, 0.0f, (float)height);

	float AspectRatio_mondo = (float)(width) / (float)(height); //Rapporto larghezza altezza di tutto ciò che è nel mondo
	 //Se l'aspect ratio del mondo è diversa da quella della finestra devo mappare in modo diverso 
	 //per evitare distorsioni del disegno
	if (AspectRatio_mondo > w / h)   //Se ridimensioniamo la larghezza della Viewport
	{
		glViewport(0, 0, w, w / AspectRatio_mondo);
		w_update = (float)w;
		h_update = w / AspectRatio_mondo;
	}
	else {  //Se ridimensioniamo la larghezza della viewport oppure se l'aspect ratio tra la finestra del mondo 
			//e la finestra sullo schermo sono uguali
		glViewport(0, 0, h * AspectRatio_mondo, h);
		w_update = h * AspectRatio_mondo;
		h_update = (float)h;
	}
}
int main(int argc, char* argv[])
{
	glutInit(&argc, argv);
	glutInitContextVersion(4, 0);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(width, height);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Shooting in the sea");

	glutDisplayFunc(drawScene);
	glutKeyboardFunc(myKeyboard);
	glutReshapeFunc(reshape);
	glutKeyboardUpFunc(keyboardReleasedEvent);
	glutTimerFunc(25, update_f, 0);
	glutTimerFunc(25, update_Cannone, 0);

	glewExperimental = GL_TRUE;
	glewInit();
	init_shader();
	init();

	//Gestione della Trasparenza
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glutMainLoop();
}