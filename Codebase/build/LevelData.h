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
		unsigned int transformOffset;			//goes to push constant
		unsigned int materialIndex;				//goes to push constant
	};

	// Members
	std::vector<UniqueMesh> uniqueMeshes;		
	std::vector<H2B::VERTEX> vertices;				//goes to vertex buffer
	std::vector<unsigned int> indices;				//goes to index buffer
	std::vector<GW::MATH::GMATRIXF> transforms;		//goes to storage buffer
	std::vector<H2B::ATTRIBUTES> materials;			//goes to storage buffer

	//unbound texture array
	//descriptor for each texture
	//swap between draws

	//layout with diffuse, normal, and specular
	//swap out descriptor sets

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

		// ALSO NOTE:
		// The way this is currently set up if you don't add instances 
		// in order, with duplicates next to each other, then it may
		// cause the transformOffset of some uniqueMeshes to no longer 
		// be valid.
		//
		
		UniqueMesh* instance = GetMesh(_meshName);
		if (instance)
		{
			auto iter = transforms.begin() + instance->transformOffset + instance->instanceCount;
			transforms.insert(iter, _matrix);
			instance->instanceCount++;
		}
		else
		{
			UniqueMesh newInstance;
			newInstance.name = _meshName;
			newInstance.instanceCount = 1;
			newInstance.transformOffset = transforms.size();
			transforms.push_back(_matrix);
			uniqueMeshes.push_back(newInstance);
		}
	}
};