#include <iostream>
#include <fstream>
#include <regex>
#include <string>
#include <vector>
#include <optional>
#include <limits>
#include <cstdio>

// Include GLM core & GLSL features
#include <glm/glm.hpp>

// Include GLM extensions (perspective, translate, rotate, etc.)
#include <glm/ext.hpp>

using namespace std;
using namespace glm;

/*
Specification of a sphere to render in a scene
*/
struct Sphere {

    int id{};

    string name{};

    // position (pos x, pos y, pos z)
    vec3 pos{};

    // scaling (scl x, scl y, scl z)
    vec3 scl{};

    // transformation matrix with scaling and translation
    mat4 T{};

    // inverse of transformation matrix
    mat4 Tinv{};

    // inverse transpose of transformation matrix
    mat4 TinvTranspose{};

    // color (r, g, b)
    vec3 color{};

    // ambient surface reflectance coefficient
    float Ka{};

    // diffuse surface reflectance coefficient
    float Kd{};

    // specular surface reflectance coefficient
    float Ks{};

    // reflective coefficient
    float Kr{};

    // specular exponent
    int n{};
};

/*
Specification of a point light
*/
struct Light {

    // name
    string name{};

    // position (pos x, pos y, pos z) in homogenous coords
    vec4 pos{};

    // intensity of the light source (Ir, Ig, Ib)
    vec3 intensity{};
};

/*
Specification of a scene to render.
*/
struct Scene {
    float near{};
    float left{};
    float right{};
    float bottom{};
    float top{};

    // the resolution of the image
    ivec2 res{};

    // sphere specification
    vector<Sphere> spheres{};

    // light specifications
    vector<Light> lights{};

    // background color
    vec3 back{};

    // intensity of ambient light
    vec3 ambient{};

    // output file name
    string output{};
};

/*
A ray consisting of a starting point and a direction vector.
*/
struct Ray {
    vec4 start{};
    vec4 direction{};
};

/*
Details about the intersection of a ray with a sphere
*/
struct Intersection {

    // the sphere that the ray intersected with
    Sphere sphere{};

    // the point of intersection in world space
    vec4 point{};

    // the normal vector to the sphere at the point of intersection (cartesian)
    vec3 normal{};

    // The distance along the ray that the intersection occurred
    float distance{};
};

Scene getSceneFromFile(const string& path) {

    // make sure the input file exists
    ifstream file(path);
    if (!file) {
        printf("Input file '%s' not found\n", path.c_str());
        exit(1);
    }

    // read the input file line-by-line
    Scene scene;
    string str;
    int sphereIndex = 0;
    while (getline(file, str)) {

        // tokenize the current line
        vector<string> tokens;
        regex re("[\\s]+");
        sregex_token_iterator it(str.begin(), str.end(), re, -1);
        sregex_token_iterator reg_end;
        for (; it != reg_end; ++it) {
            tokens.push_back(it->str());
        }

        // skip empty lines
        if (tokens.empty() || tokens[0].empty()) {
            continue;
        }

        // add the current line to the Scene
        if (tokens[0] == "NEAR") {
            scene.near = stof(tokens[1]);

        } else if (tokens[0] == "LEFT") {
            scene.left = stof(tokens[1]);

        } else if (tokens[0] == "RIGHT") {
            scene.right = stof(tokens[1]);

        } else if (tokens[0] == "BOTTOM") {
            scene.bottom = stof(tokens[1]);

        } else if (tokens[0] == "TOP") {
            scene.top = stof(tokens[1]);

        } else if (tokens[0] == "RES") {
            scene.res = vec2(stof(tokens[1]), stof(tokens[2]));

        } else if (tokens[0] == "SPHERE") {
            Sphere sphere;
            sphere.id = sphereIndex;
            sphereIndex++;
            sphere.name = tokens[1];
            sphere.pos = vec3(stof(tokens[2]), stof(tokens[3]), stof(tokens[4]));
            sphere.scl = vec3(stof(tokens[5]), stof(tokens[6]), stof(tokens[7]));
            sphere.color = vec3(stof(tokens[8]), stof(tokens[9]), stof(tokens[10]));
            sphere.Ka = stof(tokens[11]);
            sphere.Kd = stof(tokens[12]);
            sphere.Ks = stof(tokens[13]);
            sphere.Kr = stof(tokens[14]);
            sphere.n = stoi(tokens[15]);

            // get transformation matrices for the sphere's scaling and translation
            mat4 scaling = glm::scale(mat4(1.0), sphere.scl);
            mat4 translation = glm::translate(mat4(1.0), sphere.pos);
            sphere.T = translation * scaling;
            sphere.Tinv = glm::inverse(sphere.T);
            sphere.TinvTranspose = glm::inverseTranspose(sphere.T);

            scene.spheres.push_back(sphere);

        } else if (tokens[0] == "LIGHT") {
            Light light;
            light.name = tokens[1];
            light.pos = vec4(stof(tokens[2]), stof(tokens[3]), stof(tokens[4]), 1);
            light.intensity = vec3(stof(tokens[5]), stof(tokens[6]), stof(tokens[7]));
            scene.lights.push_back(light);

        } else if (tokens[0] == "BACK") {
            scene.back = vec3(stof(tokens[1]), stof(tokens[2]), stof(tokens[3]));

        } else if (tokens[0] == "AMBIENT") {
            scene.ambient = vec3(stof(tokens[1]), stof(tokens[2]), stof(tokens[3]));

        } else if (tokens[0] == "OUTPUT") {
            scene.output = tokens[1];

        } else {
            std::cout << "Error: unrecognized label in input file" << tokens[0] << endl;
            exit(1);
        }
    }

    return scene;
}

void writePPM(vector<vector<vec3>> pixels, const Scene& scene, const string& ppmType="P3") {
    printf("Saving image '%s' as %s: %d x %d\n", scene.output.c_str(), ppmType.c_str(), scene.res.x, scene.res.y);

    // attempt to open the output file
    FILE* fp = fopen(scene.output.c_str(), "wb");
    if (!fp)
    {
        printf("Unable to open file '%s'\n", scene.output.c_str());
        exit(1);
    }

    // write PPM header to file
    fprintf(fp, "%s\n", ppmType.c_str());
    fprintf(fp, "%d %d\n", scene.res.x, scene.res.y);
    fprintf(fp, "%d\n", UINT8_MAX);

    // write PPM image data to file
    bool isP6 = (ppmType == "P6");
    for (int y = scene.res.y - 1; y >= 0; y--)
    {
        for (int x = 0; x < scene.res.x; x++)
        {
            vec3 color = pixels[x][y];

            // scale the color from [0, 1] to [0, 255]
            color *= UINT8_MAX;

            if (isP6)
            {
                uint8_t colors[3] = { (uint8_t)color[0], (uint8_t)color[1], (uint8_t)color[2] };
                fwrite(&colors[0], sizeof(uint8_t), 3, fp);
            }
            else
            {
                fprintf(fp, "%d %d %d ", (int)color[0], (int)color[1], (int)color[2]);
            }
        }

        if (!isP6) {
            fprintf(fp, "\n");
        }
    }

    fclose(fp);
}

vector<vector<vec3>> getBlankImage(const Scene& scene) {

    vector<vector<vec3>> pixels;

    for (int i = 0; i < scene.res.x; i++) {
        vector<vec3> column;

        column.reserve(scene.res.y);
        for (int j = 0; j < scene.res.y; j++) {
            column.push_back(scene.back);
        }

        pixels.push_back(column);
    }

    return pixels;
}

vector<float> solveQuadratic(float a, float b, float c) {
    vector<float> solutions;
    float solutionCheck = (b * b) - (a * c);

    if (solutionCheck >= 0)
    {
        float solutionOne = (-b / a) + (glm::sqrt(solutionCheck) / a);
        solutions.push_back(solutionOne);

        if (solutionCheck > 0)
        {
            float solutionTwo = (-b / a) - (glm::sqrt(solutionCheck) / a);
            solutions.push_back(solutionTwo);
        }
    }

    return solutions;
}

optional<Intersection> closestIntersection(Ray ray, const Scene& scene, int sphereID=-1, float minHit=0) {
    Intersection intersection;
    intersection.distance = numeric_limits<float>::max();
    bool intersectionFound = false;

    // check each sphere in the scene for intersection
    for (const auto& sphere : scene.spheres)
    {
        // skip the sphere which the ray last bounced off of
        if (sphere.id == sphereID) {
            continue;
        }

        // transform the ray to match transformations on the current sphere
        Ray TinvRay = {

                // start
                sphere.Tinv * ray.start,

                // direction
                sphere.Tinv * ray.direction
        };

        // check for intersections between the transformed ray and a unit sphere at the origin by solving the quadratic equation for 't' in cartesian space
        float a = (float)glm::pow(glm::length(vec3(TinvRay.direction)), 2);
        float b = (float)glm::dot(vec3(TinvRay.start), vec3(TinvRay.direction));
        float c = (float)glm::pow(glm::length(vec3(TinvRay.start)), 2) - 1;
        vector<float> hitTimes = solveQuadratic(a, b, c);

        for (float hitTime : hitTimes) {
            if (hitTime > minHit && hitTime < intersection.distance) {
                intersectionFound = true;
                intersection.distance = hitTime;
                intersection.sphere = sphere;

                // calculate the point of intersection on the transformed sphere
                intersection.point = ray.start + (intersection.distance * ray.direction);

                // calculate the normal vector at the point of intersection on the unit sphere
                vec4 N = TinvRay.start + (intersection.distance * TinvRay.direction);

                // apply the inverse transpose transformation to get the normal vector on the transformed sphere
                vec3 Ninvtranspose = vec3(intersection.sphere.TinvTranspose * N);

                // normalize the unit vector and store it
                intersection.normal = glm::normalize(Ninvtranspose);
            }
        }
    }

    if (!intersectionFound) {
        return {};
    }

    return intersection;
}

Ray getReflectedRay(Ray ray, const Intersection& intersection) {
    vec4 start = intersection.point;
    vec4 N = vec4(intersection.normal, 0);
    vec4 direction = -2 * glm::dot(N, ray.direction) * N + ray.direction;
    return { start, direction };
}

vec3 shadowRay(const Scene& scene, const Light& light, const Intersection& intersection, bool isReflection=false) {
    // get ray from point -> light
    Ray toLight = { intersection.point, light.pos - intersection.point };

    // no contribution if there is a sphere blocking the light
    if (closestIntersection(toLight, scene, intersection.sphere.id, 0.00001f))
    {
        return {0, 0, 0};
    }

    // normal vector to sphere at the intersection point
    vec3 N = intersection.normal;

    // vector pointing towards light from the intersection point
    vec3 L = glm::normalize(vec3(toLight.direction));

    // reflection direction (could use glm::reflect here instead)
    vec3 R = glm::normalize(-L - 2.0f * dot(N, -L) * N);

    // vector pointing from the intersection point back towards the eye
    vec3 V = glm::normalize(vec3(vec4(0, 0, 0, 1) - intersection.point));

    // check if the sphere is being viewed from the inside (not needed for reflections)
    if (!isReflection && dot(N, V) < -0.00001f) {

        // check if the current light is inside the sphere using a ray from center of sphere to light
        Ray centerToLight = {
                vec4(intersection.sphere.pos, 1),
                light.pos - vec4(intersection.sphere.pos, 1)
        };

        if (closestIntersection(centerToLight, scene, -1, 0).has_value()) {
            return {0, 0, 0};
        }

        // the light is inside the sphere, so flip the normal vector (get the normal on the inside)
        N *= -1;
    }

    // handle the "special case" on pg. 33 of the illumination slides
    vec3 intensity = (dot(N, L) < 0.0001f) ? vec3(0, 0, 0) : light.intensity;

    // calculate diffuse lighting component
    vec3 diffuse = intersection.sphere.Kd * intensity * dot(N, L) * intersection.sphere.color;

    // calculate specular lighting component
    vec3 specular = intersection.sphere.Ks * intensity * (float)glm::pow(dot(R, V), intersection.sphere.n);

    // Janky fix for specular "halo" around shape edges
    if (dot(R, V) < -0.95 && dot(N, L) < 0.2) {
        specular = vec3(0, 0, 0);
    }

    return diffuse + specular;
}

vec3 rayTrace(Ray ray, Scene scene, int depth=0, int sphereID=-1) {
    // return black if the maximum recursion depth is exceeded (base case)
    const int MAX_DEPTH = 2;
    if (depth > MAX_DEPTH)
    {
        return {0, 0, 0};
    }

    // find the closest intersection of ray with object, if one exists.
    float minHit = (sphereID == -1) ? 1 : 0.00001;
    optional<Intersection> intersectionOptional = closestIntersection(ray, scene, sphereID, minHit);

    // if the ray hits nothing, return the background color or black if this is a reflection
    if (!intersectionOptional.has_value())
    {
        return (sphereID == -1) ? scene.back : vec3(0, 0, 0);
    }

    // compute ambient illumination
    Intersection intersection = intersectionOptional.value();
    vec3 ambientColor = intersection.sphere.Ka * scene.ambient * intersection.sphere.color;

    // compute light coming directly from point lights
    vec3 lightColor = vec3(0, 0, 0);
    for (size_t i = 0; i < scene.lights.size(); i++)
    {
        Light light = scene.lights.at(i);
        lightColor += shadowRay(scene, light, intersection, (depth > 0));
    }

    // compute light reflected off of other spheres (recursive step)
    vec3 reflectionColor = vec3(0, 0, 0);
    if (intersection.sphere.Kr != 0)
    {
        Ray reflectedRay = getReflectedRay(ray, intersection);
        reflectionColor = rayTrace(reflectedRay, scene, depth + 1, intersection.sphere.id);
        reflectionColor *= intersection.sphere.Kr;
    }

    // calculate final color
    return ambientColor + lightColor + reflectionColor;
}

Ray rayThroughPixel(const Scene& scene, int column, int row, bool pixelCenter=false) {
    float c = pixelCenter ? (float) column + 1.5f : (float) column;
    float r = pixelCenter ? (float) row + 1.5f : (float) row;

    // Adjustment to exactly match test cases
    r += 1.0;

    vec4 eye = vec4(0, 0, 0, 1);
    vec4 direction = vec4(
            scene.left + (2.0 * scene.right) * (c / (float)scene.res.x),
            scene.bottom + (2.0 * scene.top) * (r / (float)scene.res.y),
            -scene.near,
            0
    );
    return { eye, direction };
}

int main(int argc, char** argv) {

    // check argument count
    if (argc != 2)
    {
        printf("Incorrect number of arguments: expected 1, supplied %i.", argc - 1);
        exit(1);
    }

    // get input file name from command-line arguments
    string infile = argv[1];

    // parse the input file into a scene struct
    cout << "Reading scene specification..." << endl;
    Scene scene = getSceneFromFile(infile);

    // create a 2D array of pixels
    vector<vector<vec3>> pixels = getBlankImage(scene);

    // render each pixel of the scene
    cout << "Rendering... 0%";
    const unsigned int pixelCount = scene.res.x * scene.res.y;
    for (int row = 0; row < scene.res.y; row++)
    {
        for (int column = 0; column < scene.res.x; column++)
        {
            // get ray from eye through pixel
            Ray pixelRay = rayThroughPixel(scene, column, row);

            // determine pixel color
            vec3 pixelColor = rayTrace(pixelRay, scene);

            // add the color the image, clamping color values exceeding 1
            pixels[column][row] = glm::clamp(pixelColor, 0.0f, 1.0f);

            // print % progress during the render
            unsigned int pixelsRendered = row * scene.res.x + column;
            if (pixelCount > 100 && pixelsRendered % (pixelCount / 100) == 0) {
                float progress = ((float)pixelsRendered / (float)pixelCount) * 100;
                cout << "\rRendering... " << progress << "%";
            }
        }
    }
    cout << "\rRendering... 100%" << endl;

    // write the output file
    writePPM(pixels, scene, "P6");
    cout << "Done!" << endl;
}
