#include <string>
#include <GL/glew.h>
#include <glm/glm.hpp>

class HeightField {
public:
	int m_meshResolution; // triangles edges per quad side
	GLuint m_texid_hf;
	GLuint m_texid_diffuse;
	GLuint m_vao;
	GLuint m_positionBuffer;
	GLuint m_uvBuffer;
	GLuint m_indexBuffer;
	GLuint m_numIndices;
	std::string m_heightFieldPath;
	std::string m_diffuseTexturePath;

	float m_reflectivity = 0.2f;
	float m_metalness = 0.0f;
	float m_fresnel = 0.04f;
	float m_shininess = 0.0f;

	HeightField(void);

	// load height field
	void loadHeightField(const std::string &heigtFieldPath);

	// load diffuse map
	void loadDiffuseTexture(const std::string &diffusePath);

	// generate mesh
	void generateMesh(int tesselation);

	// render height map
	void draw(GLuint shader, const glm::mat4& viewMat, const glm::mat4& projMat, const float env_mult, bool use_ssao);

};
