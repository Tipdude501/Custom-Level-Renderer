#pragma once
#include "h2bParser.h"
#include "../Gateware/Gateware.h"

class LevelData
{
public:
	struct UniqueMesh {
		std::string name;
		unsigned int indexCount;
		unsigned int instanceCount = 1;
		unsigned int firstIndex;
		unsigned int vertexOffset;
		unsigned int materialIndex;
		std::vector<GW::MATH::GMATRIXF> matrices;
	};

	// Members
	std::vector<UniqueMesh> uniqueMeshes;
	std::vector<H2B::VERTEX> vertices;
	std::vector<unsigned int> indices;
	std::vector<H2B::MATERIAL> materials;

	// Returns a pointer to a unique mesh if it exists
	UniqueMesh* GetMesh(const std::string& _meshName)
	{
		for (unsigned int i = 0; i < uniqueMeshes.size(); i++)
		{
			if (uniqueMeshes[i].name.compare(_meshName) == 0)
				return &uniqueMeshes[i];
		}
		return nullptr;
	}

	// Add an instance of a unique mesh OR create a new unique mesh if does not exist
	void AddInstance(std::string _meshName, GW::MATH::GMATRIXF _matrix)
	{
		// NOTE: 
		// There is currently an instance-per-draw limit of 1024 
		// in the pixel and vertex shaders. This AddInstance setup is
		// currently designed to have no limit on instance count and 
		// will cause bugs if there are more than 1024 duplicates of an
		// object in a scene.
		//
		
		UniqueMesh* instance = GetMesh(_meshName);
		if (instance)
		{
			instance->instanceCount++;
			instance->matrices.push_back(_matrix);
		}
		else
		{
			UniqueMesh newInstance;
			newInstance.name = _meshName;
			newInstance.instanceCount = 1;
			newInstance.matrices.push_back(_matrix);
			uniqueMeshes.push_back(newInstance);
		}
	}
};

//////
//	Vertex Buffer
//	all combined vertices

//////
//	Index Buffer
//	all combined indices

/////
//	Uniform Buffer
//	scene uniforms
//		light direction
//		light color
//		view matrix
//		projection matrix
//	instance world matrices
//		use gl_InstanceIndex to access
//		MAX_INSTANCE_COUNT

//////
//	Storage buffer
//	mesh/materail data

//////
//	for lvlData.uniqueMeshse.size()
//		drawIndexed(command_buffer, lvlData.uniqueMeshes[i].indexCount,
//			lvlData.uniqueMeshes[i].instanceCount, lvlData.uniqueMeshes[i].firstIndex,
//			lvlData.uniqueMeshes[i].vertexOffset, 0);
// pass matrices, 

//////
//	LevelData
//	


//add to and then resize afterwards
//batches have transformations applied to them, baked in
//we want to parse through the Game
//vector of instances, each instance has a 
//mesh name, vector of matrices
//using that mesh name,


//for each lvlData.Instance
//drawIndexed(command_buffer, lvlData.Instance[i].indexCount,
//	lvlData.Instance[i].instanceCount, lvlData.Instance[i].firstIndex,
//	lvlData.Instance[i].vertexOffset, 0);



/*
GameLevel.txt - contains names and matrices
Parser needs


vector of UniqueMesh's
GameLevel reader pushes back for unique names, adding matrices
	parse till we find a mesh and get a name,
	search vector of UniqueMesh's if name exists
	if so, then push back matrix to the matrix vector
	else push back new UniqueMesh
h2bParser pushes vertices, indices, etc.
	for each UniqueMesh
		filename = "../Assets/Models" + um.name + ".h2b"
		parse
		write vertex data duplicates for um.matrix.size()
batch function converts UniqueMesh to Instance, and pushes back all data into LevelData



uniform buffer for instance matrices
instance has all the matrices still






https://stackoverflow.com/questions/54619507/whats-the-correct-way-to-implement-instanced-rendering-in-vulkan
in your code:

vkCmdDrawIndexed(command_buffer, indices_size,
	instance_count, 0, 0, instance_first_index);

instance_count: number of instances to draw
instance_first_index: first instance will have this id
in your vertex shader you can then use variable
gl_InstanceIndex which contains the instance id starting with instance_first_index

https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkCmdDrawIndexed.html


for every draw call, multiply by the world[gl_InstanceIndex]

for each lvlData.Instance
	drawIndexed(command_buffer, lvlData.Instance[i].indexCount,
		lvlData.Instance[i].instanceCount, lvlData.Instance[i].firstIndex,
		lvlData.Instance[i].vertexOffset, 0);

*/
