#include <ctime>
#include <iostream>
#include <sstream>
#include "FileUtils.h"
#include "ImageContainers/DCCImageContainer.h"
#include "ImageContainers/DT1ImageContainer.h"
#include <physfs.h>
#include "PhysFSStream.h"
#include "StreamReader.h"
#include "ds1.h"
#include <set>

static void strReplace(std::string& str, std::string from, const std::string& to, bool caseSensitive=true)
{
    if(from.empty())
        return;
    size_t start_pos = 0;
    std::string strCase = str;
    if (!caseSensitive)
    {
        std::transform(strCase.begin(), strCase.end(), strCase.begin(), ::tolower);
        std::transform(from.begin(), from.end(), from.begin(), ::tolower);
    }
    while((start_pos = strCase.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

static PaletteArray loadPalette(std::string filename)
{
    sf::PhysFSStream paletteFile(filename);
    assert(!paletteFile.hasError());
    std::vector<uint8_t> paletteData;
    paletteData.resize((size_t)paletteFile.getSize());
    paletteFile.read(paletteData.data(), paletteFile.getSize());
    LittleEndianStreamReader paletteStream(paletteData.data(), paletteData.size());

    PaletteArray palette;
    for (auto& c : palette)
    {
        paletteStream.read(c.b);
        paletteStream.read(c.g);
        paletteStream.read(c.r);
        c.a = (c.b || c.g || c.r) ? 255 : 0;
    }
    return palette;
}

class ObjectIdTable
{
public:
    struct Entry
    {
        int act;
        int type;
        int id;
        std::string description;
        int objectId;
    };

    ObjectIdTable(std::string filename)
    {
        sf::PhysFSStream objFile(filename);
        assert(!objFile.hasError());
        std::string objData;
        objData.resize((size_t)objFile.getSize());
        objFile.read(objData.data(), objFile.getSize());
        std::stringstream objDataStream;
        objDataStream << objData;

        std::string line;
        std::getline(objDataStream, line); // Skip header.
        while (std::getline(objDataStream, line))
        {
            struct Entry entry;
            char tempStr[128];
            int read = sscanf(line.c_str(), "%d\t%d\t%d\t%127[^\t]\t%d",
                    &entry.act, &entry.type, &entry.id, tempStr, &entry.objectId);
            assert(read == 5);
            entry.description = tempStr;
            table.push_back(entry);
        }
    }

    const Entry* get(int act, int type, int id) const
    {
        for (const auto& entry : table)
            if (entry.act == act && entry.type == type && entry.id == id)
                return &entry;
        return nullptr;
    }

private:
    std::vector<Entry> table;
};

class ObjectInfoTable
{
public:
    struct Entry
    {
        std::string name;
        std::string description;
        int id;
        std::string token;
        // TODO: many many more fields...
    };

    ObjectInfoTable(std::string filename)
    {
        sf::PhysFSStream objFile(filename);
        assert(!objFile.hasError());
        std::string objData;
        objData.resize((size_t)objFile.getSize());
        objFile.read(objData.data(), objFile.getSize());
        std::stringstream objDataStream;
        objDataStream << objData;

        std::string line;
        std::getline(objDataStream, line); // Skip header.
        while (std::getline(objDataStream, line))
        {
            struct Entry entry;
            memset(&entry, 0, sizeof(entry));
            char tempStr[3][128];
            int read = sscanf(line.c_str(), "%127[^\t]\t%127[^\t]\t%d\t%127[^\t]\t",
                    tempStr[0], tempStr[1], &entry.id, tempStr[2]);
            assert(read == 4);
            entry.name = tempStr[0];
            entry.description = tempStr[1];
            entry.token = tempStr[2];
            table.push_back(entry);
        }
    }

    const Entry* get(int id) const
    {
        if (id < table.size())
        {
            const Entry* entry = &table[id];
            // entry.id is just be the index/line number.
            assert(entry->id == id);
            return entry;
        }
        return nullptr;
    }

private:
    std::vector<Entry> table;
};

class Ds1Test
{
public:
    typedef std::vector<std::vector<int>> TileLayer;
    struct LvlObject
    {
        LvlObject(int xSubtile, int ySubtile, std::string dccFilename) : dcc(dccFilename)
        {
            this->xSubtile = xSubtile;
            this->ySubtile = ySubtile;
        }
        int xSubtile;
        int ySubtile;
        DCCImageContainer dcc;
    };

    Ds1Test(std::string ds1Filename) :
        ds1(ds1Filename), objectIdTable("obj.txt"),
        objectInfoTable ("data/global/excel/objects.txt")
    {
        std::vector<std::string> dt1Filenames;
        for (const auto& filename : ds1.files)
        {
            auto newFilename = FileUtils::getFileNameWithoutExt(filename) + ".dt1";
            strReplace(newFilename, "\\", "/");
            strReplace(newFilename, "/d2/data/", "/data/", false);
            dt1Filenames.push_back(newFilename);
        }

        //std::stringstream ss;
        //ss << "version: " << ds1.version << std::endl <<
        //    "width: " << ds1.width << std::endl <<
        //    "height: " << ds1.height << std::endl <<
        //    "act: " << ds1.act << std::endl <<
        //    "tagType: " << ds1.tagType << std::endl <<
        //    "numFiles: " << ds1.numFiles << std::endl <<
        //    "Files:" << std::endl;
        //for (int i = 0; i < ds1.files.size(); i++)
        //    ss << "    " << ds1.files[i] << " -> " << dt1Filenames[i] << std::endl;
        //ss << "numWalls: " << ds1.numWalls << std::endl <<
        //    "numFloors: " << ds1.numFloors << std::endl <<
        //    "numTags: " << ds1.numTags << std::endl <<
        //    "numShadows: " << ds1.numShadows << std::endl;
        //std::cout << ss.str();

        for (const auto& filename : dt1Filenames)
            dt1s.emplace_back(filename);

        std::stringstream ssLvlPaletteFilename;
        ssLvlPaletteFilename << "data/global/palette/ACT" << ds1.act << "/pal.dat";
        lvlPalette = loadPalette(ssLvlPaletteFilename.str());
        objectPalette = loadPalette("data/global/palette/Units/pal.dat");

        tileMap = createTileMap();
        objectMap = createObjectMap();
    }

    sf::Image2 drawLevel(int animationIndex = 0, unsigned int randomTileSeed = 0) const
    {
        sf::Image2 lvlImage;
        lvlImage.create(
                (DT1::Tile::WIDTH / 2) * (ds1.width + ds1.height),
                (DT1::Tile::HEIGHT / 2) * (ds1.width + ds1.height),
                sf::Color::Transparent);

        // This is ok for now, since tiles are ordered should be same level for same seed.
        srand(randomTileSeed);

        // Draw all tiles in level.
        for (auto& tileLayer : tileMap)
            for (int y = 0; y < tileLayer.size(); y++)
                for (int x = 0; x < tileLayer[0].size(); x++)
                    if (tileLayer[y][x] != -1)
                        drawTile(lvlImage, tileLayer[y][x], x, y, animationIndex);

        // Draw objects.
        for (const auto& lvlObject : objectMap)
        {
            if (lvlObject.dcc.size() == 0)
                continue;
            auto info = DT1ImageContainer::ImageInfo();
            int idx = animationIndex % lvlObject.dcc.size();
            auto image = lvlObject.dcc.get(idx, &objectPalette, info);
            int xPixel = (DT1::Tile::WIDTH / 2) * ((float)(lvlObject.xSubtile - lvlObject.ySubtile) / DT1::Tile::SUBTILE_SIZE + (ds1.height - 1));
            int yPixel = (DT1::Tile::HEIGHT / 2) * (lvlObject.xSubtile + lvlObject.ySubtile) / DT1::Tile::SUBTILE_SIZE;
            xPixel += info.offset.x + DT1::Tile::WIDTH / 2;
            yPixel += info.offset.y + DT1::Tile::HEIGHT - FLOOR_TILE_RAW_HEIGHT / 2;
            lvlImage.copy(image, xPixel, yPixel, sf::IntRect(0, 0, 0, 0), true);
        }

        return lvlImage;
    }

private:
    std::vector<TileLayer> createTileMap() const
    {
        std::vector<TileLayer> tMap;
        // Lower walls
        for (int i = 0; i < ds1.numWalls; i++)
            tMap.push_back(
                createTileLayer(ds1.walls, ds1.numWalls, i, {
                    DT1::Orientation::LOWER_LEFT_WALL,
                    DT1::Orientation::LOWER_RIGHT_WALL,
                    DT1::Orientation::LOWER_NORTH_CORNER_WALL,
                    DT1::Orientation::LOWER_SOUTH_CORNER_WALL }));

        // Floors
        for (int i = 0; i < ds1.numFloors; i++)
            tMap.push_back(
                createTileLayer(ds1.floors, ds1.numFloors, i, {
                    DT1::Orientation::FLOOR }));

        // Shadows
        // TODO: Currently draws shadows everywhere, must be more to it...
        //for (int i = 0; i < ds1.numShadows; i++)
        //    tMap.push_back(
        //        createTileLayer(ds1.shadows, ds1.numShadows, i, {
        //            DT1::Orientation::SHADOW }));

        // TODO: Walkable

        // Walls
        for (int i = 0; i < ds1.numWalls; i++)
            tMap.push_back(
                createTileLayer(ds1.walls, ds1.numWalls, i, {
                    DT1::Orientation::LEFT_WALL,
                    DT1::Orientation::LEFT_NORTH_CORNER_WALL,
                    DT1::Orientation::LEFT_END_WALL,
                    DT1::Orientation::LEFT_WALL_DOOR,
                    DT1::Orientation::RIGHT_WALL,
                    DT1::Orientation::RIGHT_NORTH_CORNER_WALL,
                    DT1::Orientation::RIGHT_END_WALL,
                    DT1::Orientation::RIGHT_WALL_DOOR,
                    DT1::Orientation::SOUTH_CORNER_WALL,
                    DT1::Orientation::PILLAR,
                    DT1::Orientation::TREE }));

        // Roofs
        for (int i = 0; i < ds1.numWalls; i++)
            tMap.push_back(
                createTileLayer(ds1.walls, ds1.numWalls, i, {
                    DT1::Orientation::ROOF }));

        // Specials
        for (int i = 0; i < ds1.numWalls; i++)
            tMap.push_back(
                createTileLayer(ds1.walls, ds1.numWalls, i, {
                    DT1::Orientation::SPECIAL_10,
                    DT1::Orientation::SPECIAL_11 }));

        return tMap;
    }

    std::vector<struct LvlObject> createObjectMap() const
    {
        std::vector<struct LvlObject> oMap;
        for (const auto& objectPair : ds1.objects)
        {
            const auto& object = objectPair.second;
            const auto* objectId = objectIdTable.get(ds1.act, object.type, object.id);
            assert(objectId);
            if (object.type == 1)
            {
                // TODO: Monsters/NPCs.
            }
            else if (object.type == 2)
            {
                // Objects.
                // TODO: These have multiple DCCs, associated COF files and can be animated.
                const auto* objectInfo = objectInfoTable.get(objectId->objectId);
                assert(objectInfo);
                std::stringstream dccFilename;
                dccFilename << "data/global/objects/" << objectInfo->token << "/tr/" << objectInfo->token << "trlitnuhth.dcc";
                oMap.emplace_back(object.x,  object.y, dccFilename.str());
            }
        }

        return oMap;
    }

    TileLayer createTileLayer(std::map<int, DS1::Cell> cells, int increment,
            int offset, const std::set<int>& orientations) const
    {
        auto tileLayer = TileLayer();
        tileLayer.resize(ds1.height);

        // Init all tiles to unused (-1).
        for (int y = 0; y < tileLayer.size(); y++)
        {
            tileLayer[y].resize(ds1.width);
            for (int x = 0; x < tileLayer[0].size(); x++)
                tileLayer[y][x] = -1;
        }

        int index = offset;
        for (int y = 0; y < ds1.height; y++)
        {
            for (int x = 0; x < ds1.width; x++)
            {
                // int index = offset + increment * (y * ds1.width + x);
                if (cells.count(index) != 0)
                {
                    auto id = cells.at(index).id;
                    if (orientations.count(DT1::Tile::getOrientation(id)) != 0)
                        tileLayer[y][x] = id;
                }
                index += increment;
            }
        }
        return tileLayer;
    }

    void drawTile(sf::Image2& image, int32_t id, int x, int y, int animationIndex = 0) const
    {
        std::vector<const DT1::Tile*> tileOptions; // could use std::reference_wrapper
        int selectedTileIndex = -1;
        for (const auto& dt1 : dt1s)
        {
            const auto& dt1TileIndexes = dt1.getTilesById(id);
            const auto& tiles = dt1.getTiles();

            for (int idx : dt1TileIndexes)
                tileOptions.push_back(&tiles[idx]);

            // Default to last tile in 1st DT1 as in original game.
            if (selectedTileIndex == -1)
                selectedTileIndex = tileOptions.size() - 1;
        }

        if (tileOptions.size() == 0)
            return;

        if (tileOptions[0]->animated)
        {
            selectedTileIndex = animationIndex % tileOptions.size();
        }
        else
        {
            // Select random tile based on rarity (not efficient but simple).
            std::vector<int> randTileOptions;
            for (int idx = 0; idx < tileOptions.size(); idx++)
                for (int i = 0; i < tileOptions[idx]->rarity; i++)
                    randTileOptions.push_back(idx);
            if (randTileOptions.size() > 0)
                selectedTileIndex = randTileOptions[rand() % randTileOptions.size()];
        }

        const auto& tile = *tileOptions[selectedTileIndex];

        int xPixel = (DT1::Tile::WIDTH / 2) * (x - y + (ds1.height - 1));
        int yPixel = (DT1::Tile::HEIGHT / 2) * (x + y);

        // FLOOR_TILE_RAW_HEIGHT just centers the output image.
        yPixel += FLOOR_TILE_RAW_HEIGHT - tile.yOffset;
        if (!tile.orientation.isFloor())
            yPixel -= DT1::Tile::HEIGHT / 2;
        if (tile.orientation.rawValue() == DT1::Orientation::ROOF)
        {
            printf("%d\n", tile.roofHeight);
            yPixel += tile.roofHeight - DT1::Tile::HEIGHT;
        }
        yPixel = std::max(0, yPixel);

        auto info = DT1ImageContainer::ImageInfo();
        // auto tileImage = dt1.get(selectedTileIndex, &lvlPalette, info);
        auto tileImage = tile.decode(&lvlPalette);
        image.copy(tileImage, xPixel, yPixel, sf::IntRect(0, 0, 0, 0), true);

        // Draw sibling textures...
        if (tile.orientation.rawValue() == DT1::Orientation::RIGHT_NORTH_CORNER_WALL)
        {
            auto siblingTileId = DT1::Tile::createIndex(DT1::Orientation::LEFT_NORTH_CORNER_WALL, tile.mainIndex, tile.subIndex);
            drawTile(image, siblingTileId, x, y);
        }
    }

    static constexpr int FLOOR_TILE_RAW_HEIGHT = 128;
    DS1::Decoder ds1;
    std::vector<DT1ImageContainer> dt1s;
    ObjectIdTable objectIdTable;
    ObjectInfoTable objectInfoTable;
    PaletteArray lvlPalette;
    PaletteArray objectPalette;
    std::vector<TileLayer> tileMap;
    std::vector<struct LvlObject> objectMap;
};

int main(int argc, char* argv[])
{
    FileUtils::initPhysFS(argv[0]);

    const std::string_view mountPoints[] = {
        "../../d2data",
        "../"   // For "obj.txt"
    };
    for (const std::string_view mountPoint : mountPoints)
    {
        bool mounted = FileUtils::mount(mountPoint, "", true);
        printf("Mount \"%s\" %s\n", mountPoint.data(), mounted ? "successful" : "failed");
    }

#if true

    //auto ds1Filename = "data/global/tiles/ACT1/TOWN/townW1.ds1";
    auto ds1Filename = "data/global/tiles/ACT1/CATACOMB/andy3.ds1";
    //auto ds1Filename = "data/global/tiles/ACT4/Lava/lavaE.ds1";
    const int numAnimations = 1; //20;

    auto ds1Test = Ds1Test(ds1Filename);

    unsigned int seed = time(0);
    for (int i = 0; i < numAnimations; i++)
    {
        auto lvlImage = ds1Test.drawLevel(i, seed);
        std::stringstream ss;
        ss << "test" << i << ".jpg";
        bool saved = lvlImage.saveToFile(ss.str());
        printf("Save \"%s\" %s\n", ss.str().c_str(), saved ? "successful" : "failed");
    }

#else

    auto dt1Filename = "data/global/tiles/ACT1/TOWN/fence.dt1";
    // auto dt1Filename = "data/global/tiles/ACT1/TOWN/floor.dt1";
    // auto dt1Filename = "data/global/tiles/ACT1/TOWN/objects.dt1";
    // auto dt1Filename = "data/global/tiles/ACT1/TOWN/trees.dt1";
    // auto dt1Filename = "data/global/tiles/ACT1/CAVES/cave.dt1";
    // auto dt1Filename = "data/global/tiles/ACT1/Monastry/facade.dt1";

    auto container = DT1ImageContainer(dt1Filename);

    auto palette = loadPalette("data/global/palette/ACT1/pal.dat");

    for (int i = 0; i < container.size(); i++)
    {
        auto info = DT1ImageContainer::ImageInfo();
        auto image = container.get(i, &palette, info);
        std::stringstream ss;
        ss << "test" << i << ".png";
        bool saved = image.saveToFile(ss.str());
        printf("Save #%d %s\n", i, saved ? "successful" : "failed");
    }
#endif

    printf("Done\n");

    FileUtils::deinitPhysFS();
    return EXIT_SUCCESS;
}
