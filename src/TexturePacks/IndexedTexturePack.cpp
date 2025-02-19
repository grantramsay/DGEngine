#include "IndexedTexturePack.h"
#include "TextureInfo.h"

IndexedTexturePack::IndexedTexturePack(std::unique_ptr<TexturePack> texturePack_,
	bool onlyUseIndexed_) : texturePack(std::move(texturePack_)), onlyUseIndexed(onlyUseIndexed_)
{
	if (onlyUseIndexed == false)
	{
		numIndexedTextures = texturePack->size();
	}
}

bool IndexedTexturePack::getTexture(uint32_t index, TextureInfo& ti) const
{
	auto it = textureIndexes.find(index);
	if (it != textureIndexes.cend())
	{
		index = it->second;
	}
	else if (onlyUseIndexed == true)
	{
		return false;
	}
	return texturePack->get(index, ti);
}

bool IndexedTexturePack::get(uint32_t index, TextureInfo& ti) const
{
	auto it = animatedIndexes.find(index);
	if (it != animatedIndexes.cend())
	{
		index = animatedTextures[it->second].getCurrentAnimationIndex();
	}
	return getTexture(index, ti);
}

void IndexedTexturePack::update(sf::Time elapsedTime)
{
	for (auto& anim : animatedTextures)
	{
		if (anim.refresh.update(elapsedTime) == true)
		{
			if (anim.currentIndex + 1 < anim.indexes.size())
			{
				anim.currentIndex++;
			}
			else
			{
				anim.currentIndex = 0;
			}
		}
	}
}

void IndexedTexturePack::addAnimatedTexture(uint32_t animIndex,
	sf::Time refresh, const std::vector<uint32_t>& indexes)
{
	auto it = animatedIndexes.find(animIndex);
	if (it != animatedIndexes.cend())
	{
		return;
	}
	animatedIndexes[animIndex] = animatedTextures.size();
	TexturePackAnimation anim;
	anim.refresh = refresh;
	anim.indexes = indexes;
	animatedTextures.push_back(anim);
}

void IndexedTexturePack::mapTextureIndex(uint32_t mapIndex)
{
	mapTextureIndex(mapIndex, textureIndexes.size());
}

void IndexedTexturePack::mapTextureIndex(uint32_t mapIndex, uint32_t toIndex)
{
	textureIndexes[mapIndex] = toIndex;
	if (onlyUseIndexed == true)
	{
		numIndexedTextures = std::max(numIndexedTextures, toIndex + 1);
	}
	else
	{
		numIndexedTextures = std::max(numIndexedTextures, mapIndex + 1);
	}
}
