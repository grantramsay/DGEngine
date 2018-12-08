#include "LevelMap.h"
#include "PathFinder.h"
#include "Utils/EasingFunctions.h"

LevelMap::LevelMap(const std::string_view tilFileName, const std::string_view solFileName,
	Coord width_, Coord height_, int16_t defaultTile)
	: mapSize(width_, height_), tileSet(tilFileName), sol(solFileName)
{
	resize(defaultTile);
}

LevelMap::LevelMap(Coord width_, Coord height_, int16_t defaultTile)
	: mapSize(width_, height_)
{
	resize(defaultTile);
}

void LevelMap::resize(Coord width_, Coord height_, int16_t defaultTile)
{
	mapSize.x = width_;
	mapSize.y = height_;
	resize(defaultTile);
}

void LevelMap::resize(int16_t defaultTile)
{
	if (mapSize.x == std::numeric_limits<Coord>::max())
	{
		mapSize.x--;
	}
	if (mapSize.y == std::numeric_limits<Coord>::max())
	{
		mapSize.y--;
	}
	if (defaultTile >= 0)
	{
		if ((size_t)defaultTile < tileSet.size())
		{
			resize(tileSet[defaultTile]);
		}
		else
		{
			cells.resize(mapSize.x * mapSize.y, { defaultTile, -1, -1, -1, -1, 0 });
		}
	}
	else
	{
		cells.resize(mapSize.x * mapSize.y);
	}
}

void LevelMap::resize(const TileBlock& defaultTile)
{
	cells.resize(mapSize.x * mapSize.y);
	for (Coord j = 0; j < mapSize.y; j++)
	{
		for (Coord i = 0; i < mapSize.x; i++)
		{
			auto tileIdx = defaultTile.getTileIndex(i, j);
			(*this)[i][j].setTileIndex(tileIdx);
			(*this)[i][j].setTileIndex(LevelCell::SolLayer, sol.get(tileIdx));
		}
	}
}

void LevelMap::setDefaultTileSize(int32_t tileWidth_, int32_t tileHeight_) noexcept
{
	defaultTileWidth = tileWidth_;
	defaultTileHeight = tileHeight_;
	defaultBlockWidth = std::max(1, tileWidth_ / 2);
	defaultBlockHeight = std::max(1, tileHeight_ / 2);
}

uint8_t LevelMap::getTileLight(size_t layer, const LevelCell& cell) const
{
	auto tileIndex = cell.getTileIndex(layer);
	if (tileIndex < 0)
	{
		return 0;
	}
	return lightMap.get((size_t)tileIndex);
}

void LevelMap::addLight(MapCoord lightPos, LightSource lightSource)
{
	if (lightSource.maxLight == 0 ||
		lightSource.minLight > lightSource.maxLight)
	{
		return;
	}
	pendingLights.push_back({ lightPos, lightSource, false });
}

void LevelMap::removeLight(MapCoord lightPos, LightSource lightSource)
{
	if (lightSource.maxLight == 0 ||
		lightSource.minLight > lightSource.maxLight)
	{
		return;
	}
	pendingLights.push_back({ lightPos, lightSource, true });
}

void LevelMap::doLight(MapCoord lightPos, LightSource ls,
	const std::function<void(LevelCell*, uint8_t)>& doLightFunc)
{
	if (defaultSource.maxLight == 255 ||
		ls.maxLight == 0 ||
		ls.minLight > ls.maxLight)
	{
		return;
	}

	// limit real radius
	int32_t radius = std::min(128, (int32_t)ls.radius);
	int32_t radiusSquared = radius * radius;
	double range = ((double)ls.maxLight - (double)ls.minLight);

	MapCoord mapPosStart(lightPos.x - radius, lightPos.y - radius);
	MapCoord mapPosEnd(lightPos.x + radius + 1, lightPos.y + radius + 1);

	mapPosStart.x = std::max(mapPosStart.x, 0);
	mapPosStart.y = std::max(mapPosStart.y, 0);
	mapPosEnd.x = std::min(mapPosEnd.x, mapSize.x);
	mapPosEnd.y = std::min(mapPosEnd.y, mapSize.y);

	auto easingFunc = EasingFunctions::easeLinear<double>;

	switch (ls.easing)
	{
	default:
	case LightEasing::Linear:
		easingFunc = EasingFunctions::easeLinear<double>;
		break;
	case LightEasing::Sine:
		easingFunc = EasingFunctions::easeInSine<double>;
		break;
	case LightEasing::Quad:
		easingFunc = EasingFunctions::easeInQuad<double>;
		break;
	case LightEasing::Cubic:
		easingFunc = EasingFunctions::easeInCubic<double>;
		break;
	case LightEasing::Quart:
		easingFunc = EasingFunctions::easeInQuart<double>;
		break;
	case LightEasing::Quint:
		easingFunc = EasingFunctions::easeInQuint<double>;
		break;
	case LightEasing::Expo:
		easingFunc = EasingFunctions::easeInExpo<double>;
		break;
	case LightEasing::Circ:
		easingFunc = EasingFunctions::easeInCirc<double>;
		break;
	}

	MapCoord mapPos;

	for (mapPos.y = mapPosStart.y; mapPos.y < mapPosEnd.y; mapPos.y++)
	{
		for (mapPos.x = mapPosStart.x; mapPos.x < mapPosEnd.x; mapPos.x++)
		{
			auto dist_y = mapPos.y - lightPos.y;
			auto dist_x = mapPos.x - lightPos.x;
			auto xxyy = (dist_x * dist_x) + (dist_y * dist_y);
			if (xxyy <= radiusSquared)
			{
				double distance = std::sqrt(xxyy);
				auto easedLight = easingFunc(
					distance,
					(double)ls.maxLight,
					-range,
					(double)ls.radius);
				auto light = (uint8_t)std::round(easedLight);
				doLightFunc(&(*this)[mapPos], light);
			}
		}
	}
}

void LevelMap::doDefaultLight(MapCoord lightPos, LightSource lightSource)
{
	static std::function<void(LevelCell*, uint8_t)> func = &LevelCell::setDefaultLight;
	doLight(lightPos, lightSource, func);
}

void LevelMap::doLight(MapCoord lightPos, LightSource lightSource)
{
	static std::function<void(LevelCell*, uint8_t)> func = &LevelCell::addLight;
	doLight(lightPos, lightSource, func);
}

void LevelMap::undoLight(MapCoord lightPos, LightSource lightSource)
{
	static std::function<void(LevelCell*, uint8_t)> func = &LevelCell::subtractLight;
	doLight(lightPos, lightSource, func);
}

void LevelMap::initLights()
{
	for (auto& cell : cells)
	{
		cell.clearLights(defaultSource.maxLight);
	}
	if (defaultSource.maxLight == 255)
	{
		return;
	}
	LightSource ls = defaultSource;
	for (int j = 0; j < mapSize.y; j++)
	{
		for (int i = 0; i < mapSize.x; i++)
		{
			auto& cell = (*this)[i][j];
			ls.maxLight = lightMap.get(cell.getTileIndex(0));
			doDefaultLight(MapCoord(i, j), ls);
			for (auto& obj : cell)
			{
				auto lightSource = obj->getLightSource();
				addLight(obj->MapPosition(), lightSource);
			}
		}
	}
}

void LevelMap::updateLights()
{
	if (pendingLights.empty() == true)
	{
		return;
	}
	for (auto& l : pendingLights)
	{
		if (l.remove == false)
		{
			doLight(l.mapPos, l.lightSource);
		}
		else
		{
			undoLight(l.mapPos, l.lightSource);
		}
	}
	pendingLights.clear();
}

void LevelMap::setTileSetArea(Coord x, Coord y, const Dun& dun)
{
	auto dWidth = dun.Width() * 2;
	auto dHeight = dun.Height() * 2;
	for (size_t j = 0; j < dHeight; j++)
	{
		for (size_t i = 0; i < dWidth; i++)
		{
			size_t xDunIndex = i;
			size_t xTilIndex = 0;
			if ((xDunIndex % 2) != 0)
			{
				xDunIndex--;
				xTilIndex = 1;
			}
			xDunIndex /= 2;

			size_t yDunIndex = j;
			size_t yTilIndex = 0;
			if ((yDunIndex % 2) != 0)
			{
				yDunIndex--;
				yTilIndex = 1;
			}
			yDunIndex /= 2;

			int32_t dunIndex = dun[xDunIndex][yDunIndex];
			if (dunIndex < 0 || (size_t)dunIndex >= tileSet.size())
			{
				continue;
			}

			int16_t tileIndex = 0;
			if (xTilIndex)
			{
				if (yTilIndex)
					tileIndex = std::get<3>(tileSet[dunIndex]); // bottom
				else
					tileIndex = std::get<1>(tileSet[dunIndex]); // left
			}
			else
			{
				if (yTilIndex)
					tileIndex = std::get<2>(tileSet[dunIndex]); // right
				else
					tileIndex = std::get<0>(tileSet[dunIndex]); // top
			}

			auto cellX = x + (Coord)i;
			auto cellY = y + (Coord)j;

			if (cellX < 0 || cellX >= mapSize.x ||
				cellY < 0 || cellY >= mapSize.y)
			{
				continue;
			}

			auto& cell = cells[cellX + (cellY * mapSize.x)];

			if (dunIndex == -1)
			{
				cell.setTileIndex(-1);
				cell.setTileIndex(LevelCell::SolLayer, 0);
			}
			else
			{
				cell.setTileIndex(tileIndex);
				cell.setTileIndex(LevelCell::SolLayer, sol.get(tileIndex));
			}
		}
	}
}

void LevelMap::setSimpleArea(Coord x, Coord y, const Dun& dun)
{
	for (size_t j = 0; j < dun.Height(); j++)
	{
		for (size_t i = 0; i < dun.Width(); i++)
		{
			auto cellX = x + (Coord)i;
			auto cellY = y + (Coord)j;

			if (cellX < 0 || cellX >= mapSize.x ||
				cellY < 0 || cellY >= mapSize.y)
			{
				continue;
			}

			auto& cell = cells[(size_t)(cellX + (cellY * mapSize.x))];

			auto tileIndex = dun[i][j];
			cell.setTileIndex(tileIndex);
			cell.setTileIndex(LevelCell::SolLayer, (tileIndex >= 0 ? sol.get(tileIndex) : 0));
		}
	}
}

void LevelMap::setSimpleArea(Coord x, Coord y, size_t layer,
	const Dun& dun, bool normalizeSolLayer)
{
	if (layer > LevelCell::NumberOfLayers)
	{
		return;
	}
	for (size_t j = 0; j < dun.Height(); j++)
	{
		for (size_t i = 0; i < dun.Width(); i++)
		{
			auto cellX = x + (Coord)i;
			auto cellY = y + (Coord)j;

			if (cellX < 0 || cellX >= mapSize.x ||
				cellY < 0 || cellY >= mapSize.y)
			{
				continue;
			}

			auto& cell = cells[(size_t)(cellX + (cellY * mapSize.x))];

			auto tileIndex = dun[i][j];
			if (layer == LevelCell::SolLayer &&
				normalizeSolLayer == true)
			{
				tileIndex = (tileIndex != 0 ? 1 : 0);
			}
			cell.setTileIndex(layer, tileIndex);
		}
	}
}

bool LevelMap::isMapCoordValid(Coord x, Coord y) const noexcept
{
	return x >= 0 && x < mapSize.x &&
		y >= 0 && y < mapSize.y;
}

bool LevelMap::isMapCoordValid(const MapCoord& mapCoord) const noexcept
{
	return isMapCoordValid(mapCoord.x, mapCoord.y);
}

sf::Vector2f LevelMap::getCoord(const MapCoord& tile) const
{
	return getCoord(tile, defaultBlockWidth, defaultBlockHeight);
}

sf::Vector2f LevelMap::getCoord(const MapCoord& tile,
	int32_t blockWidth, int32_t blockHeight) const
{
	return sf::Vector2f(
		(float)((tile.y*(-blockWidth)) + blockWidth * tile.x + mapSize.y * blockWidth - blockWidth),
		(float)((tile.y * blockHeight) + blockHeight * tile.x)
	);
}

MapCoord LevelMap::getTile(const sf::Vector2f& coords) const noexcept
{
	return getTile(coords, defaultBlockWidth, defaultBlockHeight);
}

MapCoord LevelMap::getTile(const sf::Vector2f& coords,
	int32_t blockWidth, int32_t blockHeight) const noexcept
{
	// Position on the map in pixels
	int32_t flatX = (int32_t)coords.x;
	int32_t flatY = (int32_t)coords.y;

	// position on the map divided into (blockWidth)x(blockHeight) flat blocks
	// every second one of these blocks is centred on an isometric
	// block centre, the others are centred on isometric block corners
	int32_t flatGridX = (flatX + (blockWidth / 2)) / blockWidth;
	int32_t flatGridY = (flatY + (blockHeight / 2)) / blockHeight;

	// origin position (in flat grid coords) for the first line (isometric y = 0)
	int32_t flatOriginPosX = mapSize.y;
	int32_t flatOriginPosY = 15;

	// when a flat grid box is clicked that does not centre on an isometric block, work out which
	// isometric quadrant of that box was clicked, then adjust flatGridPos accordingly
	if ((flatGridX % 2 == 1 && flatGridY % 2 == 1) ||
		(flatGridX % 2 == 0 && flatGridY % 2 == 0))
	{
		// origin of current flat grid box
		int32_t baseX = blockWidth * flatGridX - (blockWidth / 2);
		int32_t baseY = blockHeight * flatGridY - (blockHeight / 2);

		// position within grid box
		int32_t blockPosX = flatX - baseX;
		int32_t blockPosY = flatY - baseY;

		if (blockPosY * 2 > blockPosX)
		{
			if (blockPosX < (flatOriginPosY - blockPosY) * 2)
			{
				flatGridX--;
			}
			else
			{
				flatGridY++;
			}
		}
		else
		{
			if (blockPosX < (flatOriginPosY - blockPosY) * 2)
			{
				flatGridY--;
			}
			else
			{
				flatGridX++;
			}
		}
	}

	// flatOrigin adjusted for the current y value
	int32_t lineOriginPosX = flatOriginPosX + ((flatGridX - flatOriginPosX) - (flatGridY - flatOriginPosY)) / 2;
	int32_t lineOriginPosY = flatOriginPosY - (-(flatGridX - flatOriginPosX) - (flatGridY - flatOriginPosY)) / 2;

	int32_t isoPosX = flatGridX - lineOriginPosX;
	int32_t isoPosY = flatGridY - lineOriginPosY;

	return MapCoord((Coord)isoPosX, (Coord)isoPosY);
}

void LevelMap::setOutOfBoundsTileIndex(size_t layer, int16_t tile) noexcept
{
	if (tile >= 0 && (size_t)tile < tileSet.size())
	{
		outOfBoundsTiles[layer] = tileSet[tile];
	}
	else
	{
		outOfBoundsTiles[layer] = {};
	}
}

bool LevelMap::addLevelObject(std::unique_ptr<LevelObject> obj)
{
	return obj->MapPosition(*this, obj->MapPosition());
}

bool LevelMap::removeLevelObject(const LevelObject* obj)
{
	return obj->remove(*this);
}

std::vector<MapCoord> LevelMap::getPath(const MapCoord& a, const MapCoord& b) const
{
	std::vector<MapCoord> path;

	if (a == b)
	{
		path.push_back(a);
		return path;
	}

	MapSearchNode start(*this, a.x, a.y, PlayerDirection::All);
	MapSearchNode end(*this, b.x, b.y, PlayerDirection::All);
	MapSearchNode endOrig(end);

	if (end.IsValid() == false)
	{
		return path;
	}
	if (end.IsPassable() == false)
	{
		if (((*this)[b]).hasObjects() == true)
		{
			if (start.GoalDistanceEstimateC(end) == 1.f ||
				getNearestPassableEndNode(*this, start, end) == false)
			{
				path.push_back(b);
				return path;
			}
			if ((*this)[end.x][end.y].PassableIgnoreObject() == false)
			{
				return path;
			}
		}
		else
		{
			return path;
		}
	}

	PathFinder pathFinder(this);
	pathFinder.SetStartAndGoalStates(start, end);

	unsigned int SearchState;
	do
	{
		SearchState = pathFinder.SearchStep();
		if (pathFinder.GetStepCount() == PathFinder::MaxNodes)
		{
			pathFinder.CancelSearch();
		}
	} while (SearchState == PathFinder::SEARCH_STATE_SEARCHING);

	if (SearchState == PathFinder::SEARCH_STATE_SUCCEEDED)
	{
		if (endOrig.IsPassable() == false)
		{
			path.push_back(MapCoord(endOrig.x, endOrig.y));
		}
		auto node = pathFinder.GetSolutionEnd();
		while (true)
		{
			if (node == nullptr)
			{
				break;
			}
			path.push_back(MapCoord(node->x, node->y));
			node = pathFinder.GetSolutionPrev();
		};
		pathFinder.FreeSolutionNodes();
	}
	pathFinder.EnsureMemoryFreed();

	return path;
}

std::string LevelMap::toCSV(bool zeroBasedIndex) const
{
	std::string str;
	int16_t inc = (zeroBasedIndex == true ? 0 : 1);

	for (int j = 0; j < mapSize.y; j++)
	{
		for (int i = 0; i < mapSize.x; i++)
		{
			str += Utils::toString((*this)[i][j].getTileIndex(0) + inc) + ",";
		}
		str += "\n";
	}
	str.pop_back();
	str.pop_back();
	return str;
}
