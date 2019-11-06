//=============================================================================================
// Mintaprogram: Z�ld h�romsz�g. Ervenyes 2018. osztol.
//
// A beadott program csak ebben a fajlban lehet, a fajl 1 byte-os ASCII karaktereket tartalmazhat, BOM kihuzando.
// Tilos:
// - mast "beincludolni", illetve mas konyvtarat hasznalni
// - faljmuveleteket vegezni a printf-et kiveve
// - Mashonnan atvett programresszleteket forrasmegjeloles nelkul felhasznalni es
// - felesleges programsorokat a beadott programban hagyni!!!!!!! 
// - felesleges kommenteket a beadott programba irni a forrasmegjelolest kommentjeit kiveve
// ---------------------------------------------------------------------------------------------
// A feladatot ANSI C++ nyelvu forditoprogrammal ellenorizzuk, a Visual Studio-hoz kepesti elteresekrol
// es a leggyakoribb hibakrol (pl. ideiglenes objektumot nem lehet referencia tipusnak ertekul adni)
// a hazibeado portal ad egy osszefoglalot.
// ---------------------------------------------------------------------------------------------
// A feladatmegoldasokban csak olyan OpenGL fuggvenyek hasznalhatok, amelyek az oran a feladatkiadasig elhangzottak 
// A keretben nem szereplo GLUT fuggvenyek tiltottak.
//
// NYILATKOZAT
// ---------------------------------------------------------------------------------------------
// Nev    : Stork Gabor
// Neptun : NO047V
// ---------------------------------------------------------------------------------------------
// ezennel kijelentem, hogy a feladatot magam keszitettem, es ha barmilyen segitseget igenybe vettem vagy
// mas szellemi termeket felhasznaltam, akkor a forrast es az atvett reszt kommentekben egyertelmuen jeloltem.
// A forrasmegjeloles kotelme vonatkozik az eloadas foliakat es a targy oktatoi, illetve a
// grafhazi doktor tanacsait kiveve barmilyen csatornan (szoban, irasban, Interneten, stb.) erkezo minden egyeb
// informaciora (keplet, program, algoritmus, stb.). Kijelentem, hogy a forrasmegjelolessel atvett reszeket is ertem,
// azok helyessegere matematikai bizonyitast tudok adni. Tisztaban vagyok azzal, hogy az atvett reszek nem szamitanak
// a sajat kontribucioba, igy a feladat elfogadasarol a tobbi resz mennyisege es minosege alapjan szuletik dontes.
// Tudomasul veszem, hogy a forrasmegjeloles kotelmenek megsertese eseten a hazifeladatra adhato pontokat
// negativ elojellel szamoljak el es ezzel parhuzamosan eljaras is indul velem szemben.
//=============================================================================================
#include "framework.h"

// vertex shader in GLSL: It is a Raw string (C++11) since it contains new line characters
const char * const vertexSource = R"(
	#version 330				// Shader 3.3
	precision highp float;		// normal floats, makes no difference on desktop computers

	uniform mat4 MVP;			// uniform variable, the Model-View-Projection transformation matrix
	layout(location = 0) in vec2 vp;	// Varying input: vp = vertex position is expected in attrib array 0

	void main() {
		gl_Position = vec4(vp.x, vp.y, 0, 1) * MVP;		// transform vp from modeling space to normalized device space
	}
)";

// fragment shader in GLSL
const char * const fragmentSource = R"(
	#version 330			// Shader 3.3
	precision highp float;	// normal floats, makes no difference on desktop computers
	
	uniform vec3 color;		// uniform variable, the color of the primitive
	out vec4 outColor;		// computed color of the current pixel

	void main() {
		outColor = vec4(color, 1);	// computed color is the color of the primitive
	}
)";

GPUProgram gpuProgram; // vertex and fragment shaders

class spline
{
private:
    GLuint vao, vbo;

    vec2 calculatedPoints[3600];
    std::vector<vec2> controlPoints;
    std::vector<float> controlPointPositions;
    bool modified = false;

    vec2 v(const vec2 & p1, float t1, const vec2 & p2, float t2, const vec2 & p3, float t3) const {
        return (((p3 - p2) * (1 / (t3 - t2)) + ((p2 - p1) * (1 / (t2 - t1)))) * 0.5f);
    }

    vec2 rHelper(
            const vec2 & p1, float t1, const vec2 & p2, float t2, const vec2 & p3, float t3, const vec2 & p4, float t4,
            float t
    ) const {
        vec2 v2 = v(p1, t1, p2, t2, p3, t3);
        vec2 v3 = v(p2, t2, p3, t3, p4, t4);

        vec2 a0 = p2;
        vec2 a1 = v2;
        vec2 a2 = (((p3 - p2) * 3.0f) * (1 / ((t3 - t2) * ((t3 - t2))))) -
                  ((v3 + v2 * 2.0f) * (1 / (t3 - t2)));
        vec2 a3 = (((p2 - p3) * 2.0f) * (1 / ((t3 - t2) * (t3 - t2) * (t3 - t2)))) +
                  ((v3 + v2) * (1 / ((t3 - t2) * (t3 - t2))));

        float tt0 = t - t2;
        float tt1 = tt0 * tt0;
        float tt2 = tt1 * tt0;

        return {a3 * tt2 + a2 * tt1 + a1 * tt0 + a0};

    }

    int overflow(int number) const {
        while (number < 0)
            number += controlPointPositions.size();
        while (number >= controlPointPositions.size())
            number -= controlPointPositions.size();
        return number;
    }

    vec2 calcPointFor(int number, float t) const {
        return rHelper(
                controlPoints[overflow(number - 2)], controlPointPositions[overflow(number - 2)],
                controlPoints[overflow(number - 1)], controlPointPositions[overflow(number - 1)],
                controlPoints[overflow(number)], controlPointPositions[overflow(number)],
                controlPoints[overflow(number + 1)], controlPointPositions[overflow(number + 1)],
                t
        );
    }

public:
    vec2 r(float t) const {
        if (controlPointPositions.size() < 2)
            return {0, 0};
        for (int i = 0; i < controlPointPositions.size(); ++i) {
            if (controlPointPositions[i] > t) {
                return calcPointFor(i, t);
            }
        }
        return calcPointFor(0, t);
    }

    void addPoint(vec2 && point) {
        float t = acos(dot({0,1.0}, point) / length(point)) / M_PI * 1800;
        if (point.x < 0)
            t = 3600 - t;
        auto cpIt = controlPoints.begin();
        auto cppIt = controlPointPositions.begin();
        for (int i = 0; i < controlPoints.size(); ++i) {
            if (*cppIt == t)
                return;
            if (*cppIt < t) {
                controlPointPositions.insert(cppIt, t);
                controlPoints.insert(cpIt, point);
                modified = true;
                return;
            }
            ++cpIt;
            ++cppIt;
        }
        controlPointPositions.push_back(t);
        controlPoints.push_back(point);
    }

    void Create() {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);

        for (auto & calculatedPoint : calculatedPoints) {
            calculatedPoint = {0,0};
        }

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(calculatedPoints), calculatedPoints, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    void Draw() {
        glBindVertexArray(vao);
        if (modified) {
            for (int i = 0; i < 3600; ++i) {
                calculatedPoints[i] = r((float)i);
            }
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(calculatedPoints), calculatedPoints);
            modified = false;
        }

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 3600);
    }
};

spline sp;

// Initialization, create an OpenGL context
void onInitialization() {
    glViewport(0, 0, windowWidth, windowHeight);

    sp.Create();

    gpuProgram.create(vertexSource, fragmentSource, "outColor");
}

// Window has become invalid: Redraw
void onDisplay() {
    glClearColor(1, 1, 1, 1);     // background color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear frame buffer

    // Set color to (0, 1, 0) = green
    int location = glGetUniformLocation(gpuProgram.getId(), "color");
    glUniform3f(location, 0.0f, 1.0f, 0.0f); // 3 floats

    float MVPtransf[4][4] = {1, 0, 0, 0,    // MVP matrix,
                             0, 1, 0, 0,    // row-major!
                             0, 0, 1, 0,
                             0, 0, 0, 1};

    location = glGetUniformLocation(gpuProgram.getId(), "MVP");    // Get the GPU location of uniform variable MVP
    glUniformMatrix4fv(
            location, 1, GL_TRUE, &MVPtransf[0][0]
    );    // Load a 4x4 row-major float matrix to the specified location

    sp.Draw();

    glutSwapBuffers(); // exchange buffers for double buffering
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
    if (key == 'd')
        glutPostRedisplay();         // if d, invalidate display, i.e. redraw
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
}

// Move mouse with key pressed
void onMouseMotion(
        int pX, int pY
) {
}

// Mouse click event
void onMouse(
        int button, int state, int pX, int pY
) { // pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
    // Convert to normalized device space
    float cX = 2.0f * pX / windowWidth - 1;    // flip y axis
    float cY = 1.0f - 2.0f * pY / windowHeight;

    switch (button) {
        case GLUT_LEFT_BUTTON:
            if (state == GLUT_DOWN)
                sp.addPoint({cX, cY});
            break;
    }
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
    long time = glutGet(GLUT_ELAPSED_TIME); // elapsed time since the start of the program
    glutPostRedisplay();
}
