//=============================================================================================
// Mintaprogram: Zold haromszog. Ervenyes 2018. osztol.
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
    uniform float scale;        // scale
    uniform float translateX;   // x translate
	layout(location = 0) in vec2 vp;	// Varying input: vp = vertex position is expected in attrib array 0

	void main() {
		gl_Position = vec4(vp.x * scale - translateX, vp.y * scale, 0, 1) * MVP;
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
float scale = 1.0;
float translateX = 0.0;
bool start = false;

class spline
{
private:
    GLuint vao, vbo;

    vec2 calculatedPoints[3602];
    std::vector<vec2> plainControlPoints;
    std::vector<vec2> controlPoints;
    std::vector<float> controlPointPositions;
    vec2 center;
    float area;

    std::vector<vec2> targetPoints;

    vec2 v(const vec2 & p1, float t1, const vec2 & p2, float t2, const vec2 & p3, float t3) const {
        return (((p3 - p2) * (1 / (t3 - t2)) + ((p2 - p1) * (1 / (t2 - t1)))) * 0.5f);
    }

    vec2 rHelper(
            const vec2 & p1, float t1, const vec2 & p2, float t2, const vec2 & p3, float t3, const vec2 & p4, float t4,
            float t
    ) const {
        if (t1 > t2) {
            t += 3600;
            t2 += 3600;
            t4 += 3600;
            t3 += 3600;
        }
        if (t < t2)
            t += 3600;
        if (t3 < t2)
            t3 += 3600;
        if (t4 < t2)
            t4 += 3600;

        vec2 v2 = v(p1, t1, p2, t2, p3, t3);
        vec2 v3 = v(p2, t2, p3, t3, p4, t4);

        vec2 a0 = p2;
        vec2 a1 = v2;
        vec2 a2 = (((p3 - p2) * 3.0f) * (1 / ((t3 - t2) * ((t3 - t2))))) - ((v3 + v2 * 2.0f) * (1 / (t3 - t2)));
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
                controlPoints[overflow(number + 1)], controlPointPositions[overflow(number + 1)], t
        );
    }

    vec2 calculateCenter() {
        vec2 sum = {0, 0};
        for (const auto & cp : plainControlPoints)
            sum = sum + cp;
        return sum / plainControlPoints.size();
    }

    void calculateCPPositions() {
        controlPoints.clear();
        controlPointPositions.clear();
        controlPoints.reserve(plainControlPoints.size());
        controlPointPositions.reserve(plainControlPoints.size());
        for (const auto & cp : plainControlPoints)
            addControlPoint(cp);
    }

    void addControlPoint(const vec2 & point) {
        vec2 centerTop = center + vec2{0, 1.0};
        float t = acos(dot(centerTop, point) / length(point) / length(centerTop)) / M_PI * 1800;

        if (point.x < 0)
            t = 3600 - t;
        auto cpIt = controlPoints.begin();
        auto cppIt = controlPointPositions.begin();
        for (int i = 0; i < controlPoints.size(); ++i) {
            if (*cppIt == t)
                return;
            if (*cppIt > t) {
                controlPointPositions.insert(cppIt, t);
                controlPoints.insert(cpIt, point);
                return;
            }
            ++cpIt;
            ++cppIt;
        }
        controlPointPositions.push_back(t);
        controlPoints.push_back(point);
    }

    float calculateArea() const {
        vec2 areaPolygons[100];
        for (int i = 0; i < 100; ++i) {
            areaPolygons[i] = r((float) (i * 36));
        }
        float sum1 = 0;
        float sum2 = 0;
        for (int i = 1; i < 100; ++i) {
            sum1 += areaPolygons[i - 1].x * areaPolygons[i].y;
            sum2 += areaPolygons[i].x * areaPolygons[i - 1].y;
        }
        sum1 += areaPolygons[99].x * areaPolygons[0].y;
        sum2 += areaPolygons[0].x * areaPolygons[99].y;
        return abs((sum1 - sum2) / 2);
    }

    void calculateTargetPoints() {
        targetPoints.clear();
        targetPoints.reserve(controlPoints.size());
        auto r = (float)sqrt(area / M_PI);
        for (int i = 0; i < controlPoints.size(); ++i)
            targetPoints.push_back(createPointOnCircle(r, i));
    }

    vec2 createPointOnCircle(float r, int i) const {
        float y = center.x + r * (float)cos((float)i / controlPoints.size() * 2 * M_PI);
        float x = center.y + r * (float)sin((float)i / controlPoints.size() * 2 * M_PI);
        return {x, y};
    }

    void resizeToArea() {
        float currentArea = calculateArea();
        while (area - 0.01 < currentArea && currentArea < area + 0.01) {
            float currentScale = sqrt(area/currentArea);

            for (auto & controlPoint : controlPoints) {
                controlPoint = controlPoint - center;
                controlPoint = controlPoint * scale;
                controlPoint = controlPoint + center;
            }
        }
    }

    void recenter() {
        vec2 currentCenter = calculateCenter();
        vec2 transform = center - currentCenter;
        for (auto & controlPoint : controlPoints)
            controlPoint = controlPoint + transform;
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
        plainControlPoints.push_back(point);
        center = calculateCenter();
        calculateCPPositions();
        area = calculateArea();
        calculateTargetPoints();
    }

    void create() {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &vbo);

        for (auto & calculatedPoint : calculatedPoints) {
            calculatedPoint = {0, 0};
        }

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(calculatedPoints) * 2, nullptr, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }

    void draw() {
        glBindVertexArray(vao);
        calculatedPoints[0] = center;
        for (int i = 1; i < 3601; ++i) {
            calculatedPoints[i] = r((float) (i - 1));
        }
        calculatedPoints[3601] = calculatedPoints[1];
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(calculatedPoints), calculatedPoints);
        glBufferSubData(GL_ARRAY_BUFFER, sizeof(calculatedPoints), targetPoints.size() * sizeof(vec2), targetPoints.data());

        int location = glGetUniformLocation(gpuProgram.getId(), "color");
        glUniform3f(location, 0.0f, 1.0f, 1.0f);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 3602);

        location = glGetUniformLocation(gpuProgram.getId(), "color");
        glUniform3f(location, 1.0f, 1.0f, 1.0f);
        glDrawArrays(GL_LINE_LOOP, 1, 3600);

        glDrawArrays(GL_LINE_LOOP, 3602, targetPoints.size());
    }

    void animate(long elapsedTime) {
        if (start && controlPoints.size() > 1) {
            for (int i = 0; i < controlPoints.size(); ++i) {
                vec2 dir = targetPoints[i] - controlPoints[i];
                dir = normalize(dir);
                dir = dir * 0.01;
                controlPoints[i] = controlPoints[i] + dir;
            }

            resizeToArea();
            recenter();
        }
    }
};

spline sp;

void setScale() {
    int location = glGetUniformLocation(gpuProgram.getId(), "scale");
    glUniform1f(location, scale);
}

void setTranslateX() {
    int location = glGetUniformLocation(gpuProgram.getId(), "translateX");
    glUniform1f(location, translateX);
}

// Initialization, create an OpenGL context
void onInitialization() {
    glViewport(0, 0, windowWidth, windowHeight);

    sp.create();

    gpuProgram.create(vertexSource, fragmentSource, "outColor");
}

// Window has become invalid: Redraw
void onDisplay() {
    glClearColor(0, 0, 0, 0);     // background color
    glClear(GL_COLOR_BUFFER_BIT); // clear frame buffer

    float MVPtransf[4][4] = {1, 0, 0, 0,    // MVP matrix,
                             0, 1, 0, 0,    // row-major!
                             0, 0, 1, 0, 0, 0, 0, 1};

    int location = glGetUniformLocation(gpuProgram.getId(), "MVP");    // Get the GPU location of uniform variable MVP
    glUniformMatrix4fv(
            location, 1, GL_TRUE, &MVPtransf[0][0]
    );    // Load a 4x4 row-major float matrix to the specified location

    setScale();
    setTranslateX();

    sp.draw();

    glutSwapBuffers(); // exchange buffers for double buffering
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
    if (key == 'p') {
        translateX += 0.1f;
        setTranslateX();
        glutPostRedisplay();
    } else if (key == 'z') {
        scale *= 1.1;
        setScale();
        glutPostRedisplay();
    } else if (key == 'a') {
        start = true;
        glutPostRedisplay();
    }
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

    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && !start)
        sp.addPoint({cX / scale, cY / scale});
}

long lastTime = 0;

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
    long time = glutGet(GLUT_ELAPSED_TIME); // elapsed time since the start of the program
    sp.animate(time-lastTime);
    lastTime = time;
    glutPostRedisplay();
}
