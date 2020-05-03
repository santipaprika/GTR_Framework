
#include "material.h"

#include "includes.h"
#include "texture.h"

using namespace GTR;

std::map<std::string, Material*> Material::sMaterials;

Material* Material::Get(const char* name)
{
	assert(name);
	std::map<std::string, Material*>::iterator it = sMaterials.find(name);
	if (it != sMaterials.end())
		return it->second;
	return NULL;
}

void Material::registerMaterial(const char* name)
{
	this->name = name;
	sMaterials[name] = this;
}

void Material::setUniforms(Shader* shader) {
	shader->setUniform("u_has_occlusion_texture", false);
	shader->setUniform("u_has_emissive_texture", false);

	if (color_texture)
		shader->setUniform("u_texture", color_texture, 1);

	if (emissive_texture) {
		shader->setUniform("u_emissive_texture", emissive_texture, 2);
		shader->setUniform("u_has_emissive_texture", true);
	}

	if (occlusion_texture) {
		shader->setUniform("u_occlusion_texture", occlusion_texture, 3);
		shader->setUniform("u_has_occlusion_texture", true);
	}

	shader->setUniform("u_color", color);
	shader->setUniform("u_tiles_number", tiles_number);
}
void Material::renderInMenu()
{
	#ifndef SKIP_IMGUI
	ImGui::Text("Name: %s", name.c_str()); // Show String
	ImGui::Checkbox("Two sided", &two_sided);
	ImGui::Combo("AlphaMode", (int*)&alpha_mode,"NO_ALPHA\0MASK\0BLEND",3);
	ImGui::SliderFloat("Alpha Cutoff", &alpha_cutoff, 0.0f, 1.0f);
	ImGui::ColorEdit4("Color", color.v); // Edit 4 floats representing a color + alpha
	if (color_texture && ImGui::TreeNode(color_texture, "Color Texture"))
	{
		int w = ImGui::GetColumnWidth();
		float aspect = color_texture->width / (float)color_texture->height;
		ImGui::Image((void*)(intptr_t)color_texture->texture_id, ImVec2(w, w*aspect));
		ImGui::TreePop();
	}
	#endif
}

Material::~Material()
{
	if (name.size())
	{
		auto it = sMaterials.find(name);
		if (it != sMaterials.end());
		sMaterials.erase(it);
	}
}

