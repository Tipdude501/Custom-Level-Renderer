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
		std::vector<GW::MATH::GMATRIXF> matrices;
	};

	// Members
	std::vector<UniqueMesh> uniqueMeshes;
	std::vector<H2B::VERTEX> vertices;
	std::vector<unsigned int> indices;
	std::vector<H2B::MESH> meshes;
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
