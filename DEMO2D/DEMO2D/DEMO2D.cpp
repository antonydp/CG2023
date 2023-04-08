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


int frame_animazione = 0; // usato per animare
float angoloRotazione = 0.0;
float angoloRotazione2 = 0.0;


//Acqua
typedef struct {
	float x, y, z; // position
	float r, g, b, a; // color
	float vx, vy; // velocity
} WaterParticle;

vector<WaterParticle> particles;
const int NUM_PARTICLES = 100;
const float GRAVITY = 0.01f;
const float MAX_VELOCITY = 0.15f; 

GLuint VAO_acqua;
GLuint VBO_acqua_G;
GLuint VBO_acqua_C;
mat4 acquamatrix;

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
Figura vaso;
Figura vasotop;
Figura innaffiatoio;
Figura tuboinnaffiatoio;

//SCENA
vector<Figura> Scena;
vec4 gray;
vec4 lightgray;

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

void createVAO_Acqua() {
	// Generate and bind the VAO
	glGenVertexArrays(1, &VAO_acqua);
	glBindVertexArray(VAO_acqua);
	// Generate and bind the VBO
	glGenBuffers(1, &VBO_acqua_G);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_acqua_G);
	// Copy the particle data to the VBO
	glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(WaterParticle), &particles[0], GL_STATIC_DRAW);
	
	// Configure the position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(WaterParticle), (void*)offsetof(WaterParticle, x));
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &VBO_acqua_C);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_acqua_C);
	glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(WaterParticle), &particles[0], GL_STATIC_DRAW);
	// Configure the color attribute
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(WaterParticle), (void*)offsetof(WaterParticle, r));
	glEnableVertexAttribArray(1);
	// Unbind the VAO and VBO
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

//Tolleranza Planarietà
float tol_planarita = 0.01f;

//resolution (number of segments)
int resolution;

//Array per spline
float CurveArray[300][2];
float PointArray[6][2];

vector<function<vec2(float)>> pointfun; 
vector<function<vec4(float)>> colorfun;

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
	for (j = 0; j < points.size(); j++) {
		fig->vertici.push_back(vec3(center.position, 0));
		fig->colors.push_back(center.color);
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

	PointArray[0][0] = 0.3f; PointArray[0][1] = 0.6f;
	PointArray[1][0] = 0.5f; PointArray[1][1] = 0.8f;
	PointArray[2][0] = 0.8f; PointArray[2][1] = 0.8f;
	PointArray[3][0] = 0.75f; PointArray[3][1] = 0.1f;
	PointArray[4][0] = 0.45f; PointArray[4][1] = -0.5f;
	PointArray[5][0] = 0.3f; PointArray[5][1] = -1;

	
	costruisci_spline_spessa(PointArray, 6, vec4(.58f, 0.3f, 0, 1), .02f, &stelo);
	Scena.push_back(stelo);

	center = { {0, 0}, {0.27f, 0.23f, 0, 1} };
	pointfun = {
	[](float t) -> vec2 {
		float x = t / (2 * PI) - 1.5f;
		float y = -tan(t / (2 * PI)) + 0.6f;
		return  vec2(x, y);
	} ,
	[](float t) -> vec2 {
		float x = t / (2 * PI) - 0.5f;
		float y = -1;
		return vec2(x * 1.1f, y);
	} ,
	[](float t) -> vec2 {
		float x = -t / (2 * PI) + 1.5f;
		float y = tan(-t / (2 * PI)) + 0.6f;
		return vec2(x, y);
	} ,
	[](float t) -> vec2 {
		float x = 1.5f * sin((t - PI) / 2);
		float y = 0.2f * cos((t - PI) / 2) + 0.6f;
		return  vec2(x, y);
	} };
	colorfun = { [](float t) -> vec4 {
		float r = 0.27f;
		float g = 0.23f;
		float b = 0.0f;
		return vec4(r, g, b, 1.0f);
	},
		[](float t) -> vec4 {
		float r = 0.27f;
		float g = 0.23f;
		float b = 0.0f;
		return vec4(r, g, b, 1.0f);
	} ,
		[](float t) -> vec4 {
		float r = 0.27f;
		float g = 0.23f;
		float b = 0.0f;
		return vec4(r, g, b, 1.0f);
	} ,
		[](float t) -> vec4 {
		float r = 0.27f;
		float g = 0.23f;
		float b = 0.0f;
		return vec4(r, g, b, 1.0f);
	} };
	costruisci_curva_parametrica(center, pointfun, colorfun, { 10,2,10,10 }, &vaso);
	Scena.push_back(vaso);

	center = { {0, 0.7f}, {.58f, 0.3f, 0, 1}  };
	pointfun = {
	[](float t) -> vec2 {
		float x = 1.5f * sin(t);
		float y = 0.2f * cos(t) + 0.6f;
		return  vec2(x, y);
	} };
	colorfun = {
		[](float t) -> vec4 {
		float r = 0.47f;
		float g = 0.23f;
		float b = 0.0f;
		return vec4(r, g, b, 1.0f);
	} };
	costruisci_curva_parametrica(center, pointfun, colorfun, { 10 }, &vasotop);
	Scena.push_back(vasotop);

	gray = vec4(.7, .7, .7, 1);
	lightgray = vec4(.2, .2, .2, 1);
	center = { {0, 0}, mix(gray, lightgray, 0.5f) };
	pointfun = {
		// Bottom
		[](float t) -> vec2 { return mix(vec2(-1, -1), vec2(1, -1), t / (2 * PI)); },
		// Top
		[](float t) -> vec2 { return mix(vec2(-0.8, 1), vec2(0.8, 1), t / (2 * PI)); },
		// Left
		[](float t) -> vec2 { return mix(vec2(-1, -1), vec2(-0.8, 1), t / (2 * PI)); },
		// Right
		[](float t) -> vec2 { return mix(vec2(1, -1), vec2(0.8, 1), t / (2 * PI)); },
	};
	colorfun = {
		[](float t) -> vec4 {return mix(gray, lightgray, t / (2 * PI)); },
		[](float t) -> vec4 {return mix(gray, lightgray, t / (2 * PI)); } ,
		[](float t) -> vec4 {return gray; } ,
		[](float t) -> vec4 {return lightgray; } };
	costruisci_curva_parametrica(center, pointfun, colorfun, { 10,2,10,10 }, & innaffiatoio);
	Scena.push_back(innaffiatoio);


	float lunghezzatubo = 1.0f;
	gray = vec4(.7, .7, .7, 1);
	lightgray = vec4(.2, .2, .2, 1);

	vec2 attaccotubo0 = mix(vec2(1, -1), vec2(0.8, 1), 0.8f);
	vec2 attaccotubo1 = mix(vec2(1, -1), vec2(0.8, 1), 0.9f);
	vec2 tuboorto = cross(vec3(attaccotubo0 - attaccotubo1, 0), vec3(0, 0, 1));
	tuboorto = tuboorto / length(tuboorto) * lunghezzatubo;
	center = { {attaccotubo0.x, attaccotubo0.y}, mix(gray, lightgray, 0.5f) };

	pointfun = {
		// Bottom
		[attaccotubo0, tuboorto](float t) -> vec2 { return mix(attaccotubo0, attaccotubo0 - tuboorto, t / (2 * PI)); },
		// Top
		[attaccotubo1, tuboorto](float t) -> vec2 { return mix(attaccotubo1, attaccotubo1 - tuboorto, t / (2 * PI)); },
		// Left
		[attaccotubo0, attaccotubo1](float t) -> vec2 { return mix(attaccotubo0, attaccotubo1, t / (2 * PI)); },
		// Right
		[attaccotubo0, attaccotubo1, tuboorto](float t) -> vec2 { return mix(attaccotubo0 - tuboorto, attaccotubo1 - tuboorto, t / (2 * PI)); },
	};
	colorfun = {
		[](float t) -> vec4 {return lightgray; },
		[](float t) -> vec4 {return gray; } ,
		[](float t) -> vec4 {return mix(lightgray, gray, t / (2 * PI)); },
		[](float t) -> vec4 {return mix(lightgray, gray, t / (2 * PI)); } };
	costruisci_curva_parametrica(center, pointfun, colorfun, { 10,2,10,10 }, &tuboinnaffiatoio);
	Scena.push_back(tuboinnaffiatoio);

	MatProj = glGetUniformLocation(programId, "Projection");
	MatModel = glGetUniformLocation(programId, "Model");
	loctime = glGetUniformLocation(programId, "time");

}

void initParticles() {
	particles.resize(NUM_PARTICLES);
	for (int i = 0; i < NUM_PARTICLES; i++) {
		particles[i].x = 0;
		particles[i].y = 0;
		particles[i].z = 0.0f;
		particles[i].r = 0.0f;
		particles[i].g = 0.0f;
		particles[i].b = 1.0f;							//trucco per evitare che partano tutte insieme 
		particles[i].a = 0.0f;							//partono invisibili
		particles[i].vy = ((float)rand() / RAND_MAX) ;	//sparate verso l'alto a velocità variabili
		particles[i].vx = 0;							// cosi che vengano resettate a poco a poco sull'estremità dell'annaffiatoio 
	}
}

double degtorad(double angle) {
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
	angoloRotazione2 = cos(degtorad(frame_animazione) - PI/3) * 0.1;

	glutTimerFunc(25, update_animation, 0);
	glutPostRedisplay();

}
void drawParticles() {
	glBindBuffer(GL_ARRAY_BUFFER, VBO_acqua_G);
	// Copy the particle data to the VBO
	glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(WaterParticle), &particles[0], GL_STATIC_DRAW);

	// Configure the position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(WaterParticle), (void*)offsetof(WaterParticle, x));
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, VBO_acqua_C);
	// Copy the particle data to the VBO
	glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(WaterParticle), &particles[0], GL_STATIC_DRAW);

	// Configure the position attribute
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(WaterParticle), (void*)offsetof(WaterParticle, r));
	glEnableVertexAttribArray(0);

	// Draw the particles
	glUniformMatrix4fv(MatProj, 1, GL_FALSE, value_ptr(Projection));
	glUniformMatrix4fv(MatModel, 1, GL_FALSE, value_ptr(acquamatrix));
	glBindVertexArray(VAO_acqua);
	glPointSize(4);
	glDrawArrays(GL_POINTS, 0, particles.size());
	glBindVertexArray(0);
}

void updateParticles(int value) {
	for (int i = 0; i < NUM_PARTICLES; i++) {
		WaterParticle& p = particles[i];
		p.vy -= GRAVITY;
		mat3 vel_rotation_matrix = rotate(mat4(1.0), 5 * abs(angoloRotazione), vec3(0, 0, 1));
		mat3 pos_rotation_matrix = rotate(mat4(1.0), -5 * abs(angoloRotazione), vec3(0, 0, 1));
		vec2 rotated_vel = vel_rotation_matrix * vec3(p.vx, p.vy, 0);
		vec2 rotated_pos = pos_rotation_matrix * vec3(p.x, p.y, 0);
		p.x += rotated_vel.x; //p.x += p.vx;
		p.y += rotated_vel.y; //p.y += p.vy;
		if (rotated_pos.y < -1.6f) {
			// Particle is out of bounds, reset it
			p.x = 1.8f;
			p.y = 0.8f;
			p.vx = 13 * abs(angoloRotazione) * abs(((float)rand() / RAND_MAX) * MAX_VELOCITY - MAX_VELOCITY / 2.0f);
			p.vy = 3 * abs(angoloRotazione) * ((float)rand() / RAND_MAX) * MAX_VELOCITY;

			if (abs(angoloRotazione) < .05f) {
				p.a = 0;
			}
			else {
				p.a = 1;
			}
		}
		
	}
	glutTimerFunc(25, updateParticles, 0);

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

	vec3 globaltranslate = vec3(500, 50, 0);

	vec3 globalscalevec = 0.5f * vec3(1.2f + abs(angoloRotazione2 * 2), 1.2f + abs(angoloRotazione2 * 3), 1);


	int index;


	index = 0; //sfondo
	Scena[index].Model = mat4(1.0);
	Scena[index].Model = translate(Scena[index].Model, vec3(width / 2, height / 2, 0));
	Scena[index].Model = scale(Scena[index].Model, vec3(width/2, height/2, 1.0));

	drawfan(index);


	index = 4; //vaso
	Scena[index].Model = mat4(1.0);
	Scena[index].Model = translate(Scena[index].Model, vec3(width / 7 * 5, height / 9, 0));
	Scena[index].Model = scale(Scena[index].Model, vec3(100, 100, 1.0));

	drawfan(index);

	index = 5; //vasotop
	Scena[index].Model = mat4(1.0);
	Scena[index].Model = translate(Scena[index].Model, vec3(width / 7 * 5, height / 9, 0));
	Scena[index].Model = scale(Scena[index].Model, vec3(90, 90, 1.0));

	drawfan(index);

	index = 3; //stelo
	Scena[index].Model = mat4(1.0);
	Scena[index].Model = translate(Scena[index].Model, vec3(0, 70, 0));
	Scena[index].Model = translate(Scena[index].Model, globaltranslate);
	Scena[index].Model = scale(Scena[index].Model, globalscalevec);
	Scena[index].Model = scale(Scena[index].Model, vec3(90, 130, 1.0));
	Scena[index].Model = translate(Scena[index].Model, vec3(-.3f, 1, 0));

	drawtriangles(index);
	index = 1; //fiore fuori
	Scena[index].Model = mat4(1.0);
	Scena[index].Model = translate(Scena[index].Model, globaltranslate);
	Scena[index].Model = scale(Scena[index].Model, globalscalevec);
	Scena[index].Model = scale(Scena[index].Model, vec3(10, 10, 1.0));
	Scena[index].Model = translate(Scena[index].Model, vec3(0, 32, 0));

	drawfan(index);

	index = 2; //fiore dentro
	Scena[index].Model = mat4(1.0);
	Scena[index].Model = translate(Scena[index].Model, globaltranslate);
	Scena[index].Model = scale(Scena[index].Model, globalscalevec);
	Scena[index].Model = scale(Scena[index].Model, vec3(10, 10, 1.0));
	Scena[index].Model = translate(Scena[index].Model, vec3(0, 32, 0));
	Scena[index].Model = scale(Scena[index].Model, vec3(.5, .5, 1.0));

	drawfan(index);


	index = 6;//innaffiatoio
	Scena[index].Model = mat4(1.0);
	Scena[index].Model = translate(Scena[index].Model, vec3(width / 3, height / 2, 0));
	Scena[index].Model = rotate(Scena[index].Model, -5 * abs(angoloRotazione), vec3(0, 0, 1));
	Scena[index].Model = scale(Scena[index].Model, vec3(80, 80, 1.0));

	drawfan(index);

	index = 7;//tuboinnaffiatoio
	Scena[index].Model = mat4(1.0);
	Scena[index].Model = translate(Scena[index].Model, vec3(width / 3, height / 2, 0));
	Scena[index].Model = rotate(Scena[index].Model, -5 * abs(angoloRotazione), vec3(0, 0, 1));
	Scena[index].Model = scale(Scena[index].Model, vec3(80, 80, 1.0));

	drawfan(index);
	acquamatrix = mat4(1.0);
	acquamatrix = translate(acquamatrix, vec3(width / 3, height / 2, 0));
	acquamatrix = rotate(acquamatrix, -5 * abs(angoloRotazione), vec3(0, 0, 1));
	acquamatrix= scale(acquamatrix, vec3(80, 80, 1.0));

	// Draw the particles
	drawParticles();

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
	glutCreateWindow("Mantieni il fiore in vita");

	glutDisplayFunc(drawScene);
	glutKeyboardFunc(myKeyboard);
	glutReshapeFunc(reshape);
	glutKeyboardUpFunc(keyboardReleasedEvent);
	//glutTimerFunc(25, update_f, 0);
	
	
	glutTimerFunc(25, update_animation, 0);
	glutTimerFunc(25, updateParticles, 0);

	glewExperimental = GL_TRUE;
	glewInit();
	init_shader();
	initParticles();
	createVAO_Acqua();
	init();

	//Gestione della Trasparenza
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glutMainLoop();
}
















