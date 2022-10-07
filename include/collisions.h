struct RoomWallModel
{
	float positionX;
	float positionY;
	float positionZ;

	float scaleX;
	float scaleY;
	float scaleZ;
};

bool CheckWallCollision(glm::vec4 charPos, RoomWallModel wall);
bool CheckWallYZCollision(glm::vec4 charPos, RoomWallModel wall);
bool CheckGetObjCollision(glm::vec4 charPos, RoomWallModel getObj);
bool CollisionObj(float x1, float x2, RoomWallModel GetObj);


