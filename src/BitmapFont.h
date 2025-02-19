#pragma once

#include "Alignment.h"
#include <memory>
#include "Palette.h"
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <string>
#include <vector>

class BitmapFontTexturePack;
class Game;

class BitmapFont
{
private:
	std::shared_ptr<BitmapFontTexturePack> charTextures;
	std::shared_ptr<Palette> palette;
	sf::Color defaultColor{ sf::Color::White };
	int newLine{ 0 };
	int space{ 0 };
	int tab{ 0 };
	int padding{ 0 };

	float calculateLineLength(const char* text, int horizSpaceOffset) const noexcept;

	Palette* getPalette() const noexcept;

public:
	BitmapFont(const std::shared_ptr<BitmapFontTexturePack>& charTextures_, int padding_);

	int getNewLine() const noexcept { return newLine; }

	void setColor(const sf::Color& color_) { defaultColor = color_; }

	void setPalette(const std::shared_ptr<Palette>& pal) noexcept { palette = pal; }
	bool hasPalette() const noexcept { return palette != nullptr; }

	sf::Vector2f calculateSize(const std::string& text) const;

	sf::Vector2f calculateSize(const std::string& text,
		int horizSpaceOffset, int vertSpaceOffset, unsigned* lineCount = nullptr) const;

	void updateVertexString(std::vector<sf::Vertex>& vertexText,
		const std::string& text, sf::Color color, int horizSpaceOffset,
		int vertSpaceOffset, float sizeX, HorizontalAlign align) const;

	void draw(const std::vector<sf::Vertex>& vertexText,
		const sf::Vector2f& pos, const sf::Vector2f& size,
		const Game& game, sf::RenderTarget& target) const;
};
