// 2D_SEA.cpp : Questo file contiene la funzione 'main', in cui inizia e termina l'esecuzione del programma.
#include <iostream>
#include "ShaderMaker.h"
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <vector>
#include <functional>
#include <map>

using namespace glm;
using namespace std;

static unsigned int programId, programId_text;

#define  PI   3.14159265358979323846

mat4 Projection;
GLuint MatProj, MatModel, loctime, locres;
int nv_P;

// viewport size
int width = 720;
int height = 480;


int frame_animazione = 0; // usato per animare la fluttuazione
float angoloRotazione = 0.0;

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
} Figura;

Figura sfondo;
Figura fioregrande;
Figura fiorepiccolo;
Figura stelo;

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

//Tolleranza Planarietà
float tol_planarita = 0.01f;

//resolution (number of segments)
int resolution;

//Array per spline
float CurveArray[300][2];
float PointArray[6][2];


std::vector<std::function<vec2(float)>> pointfun; 
std::vector<std::function<vec4(float)>> colorfun;

typedef struct {
	vec2 position;
	vec4 color;
} Center_info;

float distancePointLine(vec2 P, vec2 A, vec2 B) {
	vec2 AB = B - A;
	vec2 AP = P - A;
	vec2 proj_AP_AB = dot(AP, AB) / dot(AB, AB) * AB;
	vec2 dist_vec = AP - proj_AP_AB;
	return length(dist_vec);
}

pair<vector<vec2>, vector<vec2>> deCasteljauSubdivision(float Array[300][2], float t, int NumPts) {
	vector<vec2> firstSetCP(NumPts);
	vector<vec2> secondSetCP(NumPts);

	firstSetCP[0] = vec2(Array[0][0], Array[0][1]);
	secondSetCP[0] = vec2(Array[NumPts - 1][0], Array[NumPts - 1][1]);

	for (int i = 0; i < NumPts - 1; i++) {
		for (int j = 0; j < NumPts - i - 1; j++) {
			Array[j][0] = (float)(1 - t) * Array[j][0] + (float)t * Array[j + 1][0];
			Array[j][1] = (float)(1 - t) * Array[j][1] + (float)t * Array[j + 1][1];
		}
		firstSetCP[i + 1] = vec2(Array[0][0], Array[0][1]);
		secondSetCP[i + 1] = vec2(Array[NumPts - i - 2][0], Array[NumPts - i - 2][1]);
	}

	return { firstSetCP, secondSetCP };
}

void suddivisioneAdattiva(float Array[6][2], int NumPts)
{
	// Estrai Control point esterni: 
	vec2 P1(
		Array[0][0],
		Array[0][1]);

	vec2 P2(
		Array[NumPts - 1][0],
		Array[NumPts - 1][1]);

	// Test di Planarità sui Control Point Interni
	int test_planarita = 1;
	for (int i = 1; i < NumPts - 1; i++)
	{
		// Calcolo distanza tempArray[i] dalla retta
		vec2 P(
			Array[i][0],
			Array[i][1]);

		if (distancePointLine(P, P1, P2) > tol_planarita) test_planarita = 0;
	}
	if (test_planarita == 1) {
		//Disegna segmento tra control point estremi tempArray[0] , tempArray[NumPts-1]
		CurveArray[2 * resolution][0] = P1.x;		CurveArray[2 * resolution][1] = P1.y;
		CurveArray[2 * resolution + 1][0] = P2.x;	CurveArray[2 * resolution + 1][1] = P2.y;
		resolution = resolution + 1;
		return;
	}
	else {
		// Applica deCasteljau nel parametro t=0.5 salvando i CP delle due nuove curve
		pair<vector<vec2>, vector<vec2>> result = deCasteljauSubdivision(Array, 0.5f, NumPts);
		int a = 1;

		float firstArray[300][2];
		for (int i = 0; i < NumPts; i++) {
			firstArray[i][0] = (result.first)[i].x;
			firstArray[i][1] = (result.first)[i].y;
		}

		float secondArray[300][2];
		for (int i = 0; i < NumPts; i++) {
			secondArray[i][0] = (result.second)[i].x;
			secondArray[i][1] = (result.second)[i].y;
		}

		suddivisioneAdattiva(secondArray, NumPts);
		suddivisioneAdattiva(firstArray, NumPts);
	}
	return;
}

void catmullRom(float Array[6][2], int NumPts) {
	//to get an efficient drawing I'll use the suddivisioneAdattiva Function to draw the splines :D

	if (NumPts == 2) {
		suddivisioneAdattiva(Array, NumPts);
		return;
	}
	float Points[300][2];

	//Case P[0] (spline with that interpolates points 0 & 1)
	vec2 P0(Array[0][0], Array[0][1]);
	vec2 P1(Array[1][0], Array[1][1]);
	vec2 P0p = P0 + (P1 - P0) / 3.0f;
	vec2 P2(Array[2][0], Array[2][1]);
	vec2 P1m = P1 - (P2 - P0) / 6.0f;

	Points[0][0] = P0.x;	Points[0][1] = P0.y;
	Points[1][0] = P0p.x;	Points[1][1] = P0p.y;
	Points[2][0] = P1m.x;	Points[2][1] = P1m.y;
	Points[3][0] = P1.x;	Points[3][1] = P1.y;
	suddivisioneAdattiva(Points, 4);

	//case P[NumPts] (spline with that interpolates point NumPts - 2 & NumPts - 1)
	vec2 PNm2(Array[NumPts - 3][0], Array[NumPts - 3][1]);
	vec2 PNm1(Array[NumPts - 2][0], Array[NumPts - 2][1]);
	vec2 PN(Array[NumPts - 1][0], Array[NumPts - 1][1]);
	vec2 PNm1p = PNm1 + (PN - PNm2) / 6.0f;
	vec2 PNm = PN - (PN - PNm1) / 3.0f;

	Points[0][0] = PNm1.x;	Points[0][1] = PNm1.y;
	Points[1][0] = PNm1p.x;	Points[1][1] = PNm1p.y;
	Points[2][0] = PNm.x;	Points[2][1] = PNm.y;
	Points[3][0] = PN.x;	Points[3][1] = PN.y;
	suddivisioneAdattiva(Points, 4);

	//other cases
	for (int i = 1; i < NumPts - 2; i++) {
		vec2 Pim1(Array[i - 1][0], Array[i - 1][1]);
		vec2 Pip1(Array[i + 1][0], Array[i + 1][1]);
		vec2 Pip2(Array[i + 2][0], Array[i + 2][1]);
		vec2 Pi(Array[i][0], Array[i][1]);
		vec2 Pip = Pi + (Pip1 - Pim1) / 6.0f;
		vec2 Pip1m = Pip1 - (Pip2 - Pi) / 6.0f;

		Points[0][0] = Pi.x;	Points[0][1] = Pi.y;
		Points[1][0] = Pip.x;	Points[1][1] = Pip.y;
		Points[2][0] = Pip1m.x;	Points[2][1] = Pip1m.y;
		Points[3][0] = Pip1.x;	Points[3][1] = Pip1.y;
		suddivisioneAdattiva(Points, 4);
	}
}

void costruisci_curva_parametrica(Center_info center, std::vector<std::function<vec2(float)>> points, std::vector<std::function<vec4(float)>> colors, std::vector<int> numPoints, Figura* fig) {

	int i, j;
	float stepA;
	float t;
	fig->vertici.push_back(vec3(center.position,0));
	fig->colors.push_back(center.color);
	for (j = 0; j < points.size(); j++) {
		stepA = (2 * PI) / numPoints[j];
		for (i = 0; i <= numPoints[j]; i++)
		{
			t = (float)i * stepA;
			fig->vertici.push_back(vec3(points[j](t),0));
			//Colore 
			fig->colors.push_back(colors[j](t));
		}
	}
	fig->nv = fig->vertici.size();
	crea_VAO_Vector(fig);
}

void costruisci_spline_spessa(float Array[6][2], int numpts, vec4 color, float spessore, Figura* fig){
	int i;
	catmullRom(Array, numpts);

	for (i = 0; i < 2 * resolution; i = i+2)
	{
		vec3 point_i = vec3(CurveArray[i][0], CurveArray[i][1], 0);

		vec3 point_ip = vec3(CurveArray[i + 1][0], CurveArray[i + 1][1], 0);
		vec3 direction = point_i - point_ip;
		vec3 orthogonal = cross(direction, vec3(0.0f, 0.0f, 1.0f));
		orthogonal = orthogonal / length(orthogonal);

		fig->vertici.push_back(point_i + spessore * orthogonal);
		fig->vertici.push_back(point_ip + spessore * orthogonal);
		fig->vertici.push_back(point_i - spessore * orthogonal);
		fig->vertici.push_back(point_i - spessore * orthogonal);
		fig->vertici.push_back(point_ip + spessore * orthogonal);
		fig->vertici.push_back(point_ip - spessore * orthogonal);
		//Colore 
		fig->colors.push_back(color);
		fig->colors.push_back(color);
		fig->colors.push_back(color);
		fig->colors.push_back(color);
		fig->colors.push_back(color);
		fig->colors.push_back(color);

		float stepA = (2 * PI) / 10;
		float t;
		int j;
		for (j = 0; j < 10; j++) {
			t = (float)j * stepA;
			fig->vertici.push_back(point_i);
			fig->vertici.push_back(point_i + spessore * vec3(cos(t), sin(t), 0));
			fig->vertici.push_back(point_i + spessore * vec3(cos(t + stepA), sin(t + stepA), 0));
			//Colore 
			fig->colors.push_back(color);
			fig->colors.push_back(color);
			fig->colors.push_back(color);

			fig->vertici.push_back(point_ip);
			fig->vertici.push_back(point_ip + spessore * vec3(cos(t), sin(t), 0));
			fig->vertici.push_back(point_ip + spessore * vec3(cos(t + stepA), sin(t + stepA), 0));
			//Colore 
			fig->colors.push_back(color);
			fig->colors.push_back(color);
			fig->colors.push_back(color);
		}
	}


	fig->nv = fig->vertici.size();
	crea_VAO_Vector(fig);
}

void init_shader(void)
{
	GLenum ErrorCheckValue = glGetError();

	char* vertexShader = (char*)"vertexShader_Demo.glsl";
	char* fragmentShader = (char*)"fragmentShader_Demo.glsl";
	programId = ShaderMaker::createProgram(vertexShader, fragmentShader);
	glUseProgram(programId);
}

void init(void)
{
	// Define the center point of the box
	Center_info center = { {0, 0}, {0, .5, .5, 1} };
	// Define the point and color functions for the sides of the box
	pointfun = {
		// Bottom
		[](float t) -> vec2 { return glm::mix(vec2(-1, -1), vec2(1, -1), t / (2 * PI)); },
		// Top
		[](float t) -> vec2 { return glm::mix(vec2(-1, 1), vec2(1, 1), t / (2 * PI)); },
		// Left
		[](float t) -> vec2 { return glm::mix(vec2(-1, -1), vec2(-1, 1), t / (2 * PI)); },
		// Right
		[](float t) -> vec2 { return glm::mix(vec2(1, -1), vec2(1, 1), t / (2 * PI)); },
	};
	colorfun = {
		// Bottom (green)
		[](float t) -> vec4 { return vec4(0, 1, 0, 1); },
		// Top (blue)
		[](float t) -> vec4 { return vec4(0, 0, 1, 1); },
		// Left 
		[](float t) -> vec4 { return glm::mix(vec4(0, 1, 0, 1), vec4(0, 0, 1, 1), t / (2 * PI)); },
		// Right 
		[](float t) -> vec4 { return glm::mix(vec4(0, 1, 0, 1), vec4(0, 0, 1, 1), t / (2 * PI)); },
	};
	costruisci_curva_parametrica(center, pointfun, colorfun, { 2,2,2,2 }, &sfondo);
	Scena.push_back(sfondo);

	center = { {0, 0}, {0, 1, 0, 1} };
	pointfun = { [](float t) -> vec2 {
		float x = (2 + 10 * sin(10 * t)) * cos(t);
		float y = (2 + 10 * sin(10 * t)) * sin(t);
		return vec2(x, y);
	} 
	};
	colorfun = { [](float t) -> vec4 {
		float r = 0;
		float g = .25 + (sin(t) + 1.0f) / 4.0f;
		float b = 0;
		return vec4(r, g, b, 1.0f);
	} };
	fioregrande.nTriangles = 130;
	costruisci_curva_parametrica(center, pointfun , colorfun , {fioregrande.nTriangles}, &fioregrande);
	Scena.push_back(fioregrande);

	center = { {0, 0}, {1, 1, 0, 1} };
	pointfun = { [](float t) -> vec2 {
		float x = (2 + 10 * sin(10 * t)) * cos(t);
		float y = (2 + 10 * sin(10 * t)) * sin(t);
		return vec2(x, y);
	} };
	colorfun = { [](float t) -> vec4 {
		float r = 1;
		float g = 1 - (sin(t) + 1.0f) / 4.0f;
		float b = 0.0f;
		return vec4(r, g, b, 1.0f);
	} };
	fiorepiccolo.nTriangles = 130;
	costruisci_curva_parametrica(center, pointfun, colorfun, {fiorepiccolo.nTriangles},&fiorepiccolo);
	Scena.push_back(fiorepiccolo);

	PointArray[0][0] = 0; PointArray[0][1] = -1;
	PointArray[1][0] = 0.5f; PointArray[1][1] = -.6;
	PointArray[2][0] = 0; PointArray[2][1] = 1;

	
	costruisci_spline_spessa(PointArray, 3, vec4(.58f, 0.3f, 0, 1), .1f, &stelo);
	Scena.push_back(stelo);

	MatProj = glGetUniformLocation(programId, "Projection");
	MatModel = glGetUniformLocation(programId, "Model");
	loctime = glGetUniformLocation(programId, "time");

}

double  degtorad(double angle) {
	return angle * PI / 180;
} 

void update_animation(int value)
{
	//Aggiorno l'angolo di fluttuazione del cannone
	frame_animazione += 1;
	if (frame_animazione >= 360) {
		frame_animazione -= 360;

	}
	angoloRotazione = cos(degtorad(frame_animazione)) * 0.1;

	glutTimerFunc(25, update_animation, 0);
	glutPostRedisplay();

}

void drawfan(int i) {

	glUniformMatrix4fv(MatProj, 1, GL_FALSE, value_ptr(Projection));
	glUniformMatrix4fv(MatModel, 1, GL_FALSE, value_ptr(Scena[i].Model));
	glBindVertexArray(Scena[i].VAO);
	glDrawArrays(GL_TRIANGLE_FAN, 0, Scena[i].nv);
	glBindVertexArray(0);
}

void drawtriangles(int i) {
	glUniformMatrix4fv(MatProj, 1, GL_FALSE, value_ptr(Projection));
	glUniformMatrix4fv(MatModel, 1, GL_FALSE, value_ptr(Scena[i].Model));
	glBindVertexArray(Scena[i].VAO);
	glDrawArrays(GL_TRIANGLES, 0, Scena[i].nv);
	glBindVertexArray(0);
}


void drawScene(void)
{
	glClear(GL_COLOR_BUFFER_BIT);
	glPointSize(14.0);
	float time = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
	glUniform1f(loctime, time);

	vec3 globaltranslate = vec3(500, 200, 0);
	vec3 globalscalevec = vec3(1.1 , 1.1, 1);

	Scena[0].Model = mat4(1.0);

	Scena[0].Model = translate(Scena[0].Model, vec3(width / 2, height / 2, 0));
	Scena[0].Model = scale(Scena[0].Model, vec3(width/2, height/2, 1.0));

	drawfan(0);

	Scena[1].Model = mat4(1.0);
	Scena[1].Model = translate(Scena[1].Model, globaltranslate);
	Scena[1].Model = scale(Scena[1].Model, globalscalevec);
	Scena[1].Model = scale(Scena[1].Model, vec3(10, 10, 1.0));
	Scena[1].Model = translate(Scena[1].Model, vec3(2, 0, 0));

	drawfan(1);

	Scena[2].Model = mat4(1.0);
	Scena[2].Model = translate(Scena[2].Model, globaltranslate);
	Scena[2].Model = scale(Scena[2].Model, globalscalevec);
	Scena[2].Model = scale(Scena[2].Model, vec3(10, 10, 1.0));
	Scena[2].Model = translate(Scena[2].Model, vec3(2, 0, 0));
	Scena[2].Model = scale(Scena[2].Model, vec3(.5, .5, 1.0));

	drawfan(2);

	Scena[3].Model = mat4(1.0);
	Scena[3].Model = translate(Scena[3].Model, vec3(width / 2, height / 2, 0));
	Scena[3].Model = scale(Scena[3].Model, vec3(width / 5, height / 5, 1.0));

	drawtriangles(3);



	glutSwapBuffers();
}

void keyboardReleasedEvent(unsigned char key, int x, int y)
{
	{
		switch (key)
		{
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
		default:
			break;
		}
	}
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
	}
	else {  //Se ridimensioniamo la larghezza della viewport oppure se l'aspect ratio tra la finestra del mondo 
			//e la finestra sullo schermo sono uguali
		glViewport(0, 0, h * AspectRatio_mondo, h); 
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
	//glutTimerFunc(25, update_f, 0);
	
	
	glutTimerFunc(25, update_animation, 0);

	glewExperimental = GL_TRUE;
	glewInit();
	init_shader();
	init();

	//Gestione della Trasparenza
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glutMainLoop();
}