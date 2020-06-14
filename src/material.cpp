
#include "material.h"

#include "includes.h"
#include "texture.h"
#include "utils.h"
#include "application.h"

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

void Material::setUniforms(Shader* shader, bool is_first_pass) {
	
	Texture* black_tex = Texture::getBlackTexture();
	Texture* white_tex = Texture::getWhiteTexture();

	//if (color) color = Vector4(1.0, 1.0, 1.0, 1.0);
	if (Application::instance->use_gamma_correction)
		shader->setUniform("u_color", Vector4(gamma(color.xyz()), color.w));
	else
		shader->setUniform("u_color", color);

	if (!color_texture || !is_first_pass)
		shader->setTexture("u_texture", white_tex, 1);
	else
		shader->setTexture("u_texture", color_texture, 1);

	if (!emissive_texture || !is_first_pass)
		shader->setTexture("u_emissive_texture", black_tex, 2);
	else
		shader->setTexture("u_emissive_texture", emissive_texture, 2);

	if (!occlusion_texture || !is_first_pass)
		shader->setTexture("u_occlusion_texture", white_tex, 3);
	else
		shader->setTexture("u_occlusion_texture", occlusion_texture, 3);

	shader->setUniform("u_color", Vector4(gamma(color.xyz()), color.w));
	shader->setUniform("u_tiles_number", tiles_number);

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", alpha_mode == GTR::AlphaMode::MASK ? alpha_cutoff : 0);

}
void Material::renderInMenu()
{
	#ifndef SKIP_IMGUI
	ImGui::Text("Name: %s", name.c_str()); // Show String
	ImGui::SliderFloat("Tile Number", &tiles_number, 0.1, 100);
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

