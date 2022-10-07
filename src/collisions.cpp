#include <glm/vec4.hpp>
#include "collisions.h"

bool CheckWallCollision(glm::vec4 charPos, RoomWallModel wall)
{
    float wallFirstX = wall.positionX - wall.scaleX;
    float wallLastX = wall.positionX + wall.scaleX;
    float epsilon = 0.5;

    if((charPos.x >= wallFirstX && charPos.x <= wallLastX)
      && std::abs(wall.positionZ - charPos.z) < epsilon)
        return true;
    return false;
}

bool CheckWallYZCollision(glm::vec4 charPos, RoomWallModel wall)
{
    float wallFirstZ = wall.positionZ - wall.scaleX;
    float wallLastZ = wall.positionZ + wall.scaleX;
    float epsilon = 0.5;

    if((charPos.z >= wallFirstZ && charPos.z <= wallLastZ) && std::abs(wall.positionX - charPos.x) < epsilon)
        return true;
    return false;
}

bool CheckGetObjCollision(glm::vec4 charPos, RoomWallModel getObj)
{
    float getObjFirstZ = getObj.positionZ - getObj.scaleX;
    float getObjLastZ = getObj.positionZ + getObj.scaleX;
    float epsilon = 0.5;

    if((charPos.z >= getObjFirstZ && charPos.z <= getObjLastZ) && std::abs(getObj.positionX - charPos.x) < epsilon)
        return true;
    return false;
}

bool CollisionObj(float x1, float x2, RoomWallModel getObj)
{
    if(x1 <= getObj.positionX && x2 >= getObj.positionX)
        return true;
    return false;
}
