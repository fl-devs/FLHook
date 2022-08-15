#include "Global.hpp"

namespace Hk::Math
{
	/** Calculate the distance between the two vectors */
	float HkDistance3D(Vector v1, Vector v2)
	{
		float sq1 = v1.x - v2.x, sq2 = v1.y - v2.y, sq3 = v1.z - v2.z;
		return sqrt(sq1 * sq1 + sq2 * sq2 + sq3 * sq3);
	}

	/** Calculate the distance between the two vectors */
	float HkDistance3DByShip(uint iShip1, uint iShip2)
	{
		Vector v1;
		Matrix m1;
		pub::SpaceObj::GetLocation(iShip1, v1, m1);
		Vector v2;
		Matrix m2;
		pub::SpaceObj::GetLocation(iShip2, v2, m2);

		return HkDistance3D(v1, v2);
	}

	Quaternion HkMatrixToQuaternion(Matrix m)
	{
		Quaternion quaternion;
		quaternion.w = sqrt(max(0, 1 + m.data[0][0] + m.data[1][1] + m.data[2][2])) / 2;
		quaternion.x = sqrt(max(0, 1 + m.data[0][0] - m.data[1][1] - m.data[2][2])) / 2;
		quaternion.y = sqrt(max(0, 1 - m.data[0][0] + m.data[1][1] - m.data[2][2])) / 2;
		quaternion.z = sqrt(max(0, 1 - m.data[0][0] - m.data[1][1] + m.data[2][2])) / 2;
		quaternion.x = (float)_copysign(quaternion.x, m.data[2][1] - m.data[1][2]);
		quaternion.y = (float)_copysign(quaternion.y, m.data[0][2] - m.data[2][0]);
		quaternion.z = (float)_copysign(quaternion.z, m.data[1][0] - m.data[0][1]);
		return quaternion;
	}

	template<typename Str>
	Str VectorToSectorCoord(uint systemId, Vector vPos)
	{
		float scale = 1.0;
		const Universe::ISystem* iSystem = Universe::get_system(systemId);
		if (iSystem)
			scale = iSystem->NavMapScale;

		float fGridSize = 34000.0f / scale;
		int gridRefX = (int)((vPos.x + (fGridSize * 5)) / fGridSize) - 1;
		int gridRefZ = (int)((vPos.z + (fGridSize * 5)) / fGridSize) - 1;

		gridRefX = min(max(gridRefX, 0), 7);
		char scXPos = 'A' + char(gridRefX);

		gridRefZ = min(max(gridRefZ, 0), 7);
		char scZPos = '1' + char(gridRefZ);

		typename Str::value_type szCurrentLocation[100];
		if constexpr (std::is_same_v<Str, std::string>)
			_snprintf_s(szCurrentLocation, sizeof(szCurrentLocation), "%c-%c", scXPos, scZPos);
		else
			_snwprintf_s(szCurrentLocation, sizeof(szCurrentLocation), L"%C-%C", scXPos, scZPos);

		return szCurrentLocation;
	}

#define PI 3.14159265f

	// Convert radians to degrees.
	float degrees(float rad)
	{
		rad *= 180 / PI;

		// Prevent displaying -0 and prefer 180 to -180.
		if (rad < 0)
		{
			if (rad > -0.005f)
				rad = 0;
			else if (rad <= -179.995f)
				rad = 180;
		}

		// Round to two decimal places here, so %g can display it without decimals.
		float frac = modff(rad * 100, &rad);
		if (frac >= 0.5f)
			++rad;
		else if (frac <= -0.5f)
			--rad;

		return rad / 100;
	}

	// Convert an orientation matrix to a pitch/yaw/roll vector.  Based on what
	// Freelancer does for the save game.
	Vector MatrixToEuler(const Matrix& mat)
	{
		Vector x = {mat.data[0][0], mat.data[1][0], mat.data[2][0]};
		Vector y = {mat.data[0][1], mat.data[1][1], mat.data[2][1]};
		Vector z = {mat.data[0][2], mat.data[1][2], mat.data[2][2]};

		Vector vec;
		float h = (float)_hypot(x.x, x.y);
		if (h > 1 / 524288.0f)
		{
			vec.x = degrees(atan2f(y.z, z.z));
			vec.y = degrees(atan2f(-x.z, h));
			vec.z = degrees(atan2f(x.y, x.x));
		}
		else
		{
			vec.x = degrees(atan2f(-z.y, y.y));
			vec.y = degrees(atan2f(-x.z, h));
			vec.z = 0;
		}
		return vec;
	}

	void Rotate180(Matrix& rot)
	{
		rot.data[0][0] = -rot.data[0][0];
		rot.data[1][0] = -rot.data[1][0];
		rot.data[2][0] = -rot.data[2][0];
		rot.data[0][2] = -rot.data[0][2];
		rot.data[1][2] = -rot.data[1][2];
		rot.data[2][2] = -rot.data[2][2];
	}

	void TranslateY(Vector& pos, Matrix& rot, float y)
	{
		pos.x += y * rot.data[0][0];
		pos.y += y * rot.data[1][0];
		pos.z += y * rot.data[2][0];
	}

	void TranslateX(Vector& pos, Matrix& rot, float x)
	{
		pos.x += x * rot.data[0][2];
		pos.y += x * rot.data[1][2];
		pos.z += x * rot.data[2][2];
	}

	void TranslateZ(Vector& pos, Matrix& rot, float z)
	{
		pos.x += z * rot.data[0][1];
		pos.y += z * rot.data[1][1];
		pos.z += z * rot.data[2][1];
	}

	uint RgbToBgr(uint color) { return (color & 0xFF000000) | ((color & 0xFF0000) >> 16) | (color & 0x00FF00) | ((color & 0x0000FF) << 16); };

	std::wstring UintToHex(uint number, uint width, bool addPrefix)
	{
		std::wstringstream stream;
		if (addPrefix)
		{
			stream << L"0x";
		}
		stream << std::setfill(L'0') << std::setw(width) << std::hex << number;
		return stream.str();
	}
}