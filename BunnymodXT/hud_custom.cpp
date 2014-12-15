#include "stdafx.hpp"

#include "cvars.hpp"
#include "modules.hpp"
#include "hud_custom.hpp"

namespace CustomHud
{
	static const float FADE_DURATION_JUMPSPEED = 0.7f;

	static SCREENINFO si;
	static float consoleColor[3] = { 1.0f, (180 / 255.0f), (30 / 255.0f) };
	static int hudColor[3] = { 255, 160, 0 }; // Yellowish.
	static bool receivedAccurateInfo = false;
	static playerinfo player;

	static client_sprite_t *SpriteList;
	static int SpriteCount;
	static std::array<HSPRITE_HL, 10> NumberSprites;
	static std::array<wrect_t, 10> NumberSpriteRects;
	static std::array<client_sprite_t*, 10> NumberSpritePointers;
	static int NumberWidth;
	static int NumberHeight;

	template<typename T, size_t size = 3>
	static inline void vecCopy(const T src[size], T dest[size])
	{
		for (size_t i = 0; i < size; ++i)
			dest[i] = src[i];
	}

	template<typename T, size_t size = 3>
	static inline void vecCopy(const std::array<T, size> src, T dest[size])
	{
		for (size_t i = 0; i < size; ++i)
			dest[i] = src[i];
	}

	static inline double sqr(double a)
	{
		return a * a;
	}

	static inline double length(double x, double y)
	{
		return std::hypot(x, y);
	}

	static inline double length(double x, double y, double z)
	{
		return std::sqrt(sqr(x) + sqr(y) + sqr(z));
	}

	static int pow(int a, int p)
	{
		int res = 1;
		for (int i = 0; i < p; ++i)
			res *= a;
		return res;
	}

	static void UpdateScreenInfo()
	{
		si.iSize = sizeof(si);
		clientDLL.pEngfuncs->pfnGetScreenInfo(&si);
	}

	static void DrawString(int x, int y, const char* s, float r, float g, float b)
	{
		clientDLL.pEngfuncs->pfnDrawSetTextColor(r, g, b);
		clientDLL.pEngfuncs->pfnDrawConsoleString(x, y, const_cast<char*>(s));
	}

	static inline void DrawString(int x, int y, const char* s)
	{
		DrawString(x, y, s, consoleColor[0], consoleColor[1], consoleColor[2]);
	}

	static void DrawMultilineString(int x, int y, std::string s)
	{
		while (s.size() > 0)
		{
			auto pos = s.find('\n');

			DrawString(x, y, const_cast<char*>(s.substr(0, pos).c_str()));
			y += si.iCharHeight;

			if (pos != std::string::npos)
				s = s.substr(pos + 1, std::string::npos);
			else
				s.erase();
		};
	}

	static void DrawDigit(int digit, int x, int y, int r, int g, int b)
	{
		assert(digit >= 0 && digit <= 9);

		clientDLL.pEngfuncs->pfnSPR_Set(NumberSprites[digit], r, g, b);
		clientDLL.pEngfuncs->pfnSPR_DrawAdditive(0, x, y, &NumberSpriteRects[digit]);
	}

	static void DrawNumber(int number, int x, int y, int r, int g, int b)
	{
		if (number < 0)
		{
			if (number == std::numeric_limits<int>::min())
				number = 0;
			else
				number = abs(number);

			// TODO: draw a minus sign.
		}

		bool startedDrawing = false;
		// Assuming that we have a non-base 10 integer storage.
		for (int i = std::numeric_limits<int>::digits10 + 1; i > 0; --i)
		{
			auto p = pow(10, i);
			auto digit = number / p;
			if (startedDrawing || digit)
			{
				startedDrawing = true;
				DrawDigit(digit, x, y, r, g, b);
				x += NumberWidth;
				number %= p;
			}
		}

		// Draw the last digit (or zero).
		DrawDigit(number, x, y, r, g, b);
	}

	static inline void DrawNumber(int number, int x, int y)
	{
		DrawNumber(number, x, y, hudColor[0], hudColor[1], hudColor[2]);
	}

	static void UpdateColors()
	{
		consoleColor[0] = 1.0f;
		consoleColor[1] = 180 / 255.0f;
		consoleColor[2] = 30 / 255.0f;

		if (!con_color_.IsEmpty())
		{
			unsigned r = 0, g = 0, b = 0;
			std::istringstream ss(con_color_.GetString());
			ss >> r >> g >> b;

			consoleColor[0] = r / 255.0f;
			consoleColor[1] = g / 255.0f;
			consoleColor[2] = b / 255.0f;
		}

		hudColor[0] = 255;
		hudColor[1] = 160;
		hudColor[2] = 0;

		if (!y_bxt_hud_color.IsEmpty())
		{
			auto colorStr = y_bxt_hud_color.GetString();
			if (colorStr != "auto")
			{
				std::istringstream color_ss(colorStr);
				color_ss >> hudColor[0] >> hudColor[1] >> hudColor[2];
			}
		}
	}

	void Init()
	{
		SpriteList = nullptr;
	}

	void VidInit()
	{
		UpdateScreenInfo();

		int SpriteRes = (si.iWidth < 640) ? 320 : 640;

		// Based on a similar procedure from hud.cpp.
		if (!SpriteList)
		{
			SpriteList = clientDLL.pEngfuncs->pfnSPR_GetList(const_cast<char*>("sprites/hud.txt"), &SpriteCount);
			if (SpriteList)
			{
				for (client_sprite_t *p = SpriteList; p < (SpriteList + SpriteCount); ++p)
				{
					// If we have a sprite of the correct resolution which is named "number_x" where x is a digit
					if (p->iRes == SpriteRes
						&& strstr(p->szName, "number_") == p->szName
						&& *(p->szName + 8) == 0
						&& isdigit(*(p->szName + 7)))
					{
						int digit = *(p->szName + 7) - '0';
						NumberSpritePointers[digit] = p;
						NumberSpriteRects[digit] = p->rc;

						std::string path("sprites/");
						path += p->szSprite;
						path += ".spr";
						NumberSprites[digit] = clientDLL.pEngfuncs->pfnSPR_Load(path.c_str());
						
						if (!digit)
						{
							NumberWidth = p->rc.right - p->rc.left;
							NumberHeight = p->rc.bottom - p->rc.top;
						}

						EngineDevMsg("[client dll] Loaded the digit %d sprite from \"%s\".\n", digit, path.c_str());
					}
				}
			}
		}
		else
		{
			size_t i = 0;
			for (auto it = NumberSpritePointers.cbegin(); it != NumberSpritePointers.cend(); ++i, ++it)
			{
				std::string path("sprites/");
				path += (*it)->szSprite;
				path += ".spr";
				NumberSprites[i] = clientDLL.pEngfuncs->pfnSPR_Load(path.c_str());
				EngineDevMsg("[client dll] Reloaded the digit %d sprite from \"%s\".\n", i, path.c_str());
			}
		}
	}

	void Draw(float flTime)
	{
		if (!y_bxt_hud.GetBool())
			return;

		int precision = y_bxt_hud_precision.GetInt();
		if (precision > 16)
			precision = 16;

		UpdateColors();

		static float prevVel[3] = { 0.0f, 0.0f, 0.0f };
		
		if (y_bxt_hud_velocity.GetBool())
		{
			int x = -200,
				y = 0;
			if (!y_bxt_hud_velocity_pos.IsEmpty())
			{
				std::istringstream pos_ss(y_bxt_hud_velocity_pos.GetString());
				pos_ss >> x >> y;
			}

			x += si.iWidth;
			
			if (receivedAccurateInfo)
				DrawString(x, y, "Velocity:");
			else
				DrawString(x, y, "Velocity:", 1.0f, 0.0f, 0.0f);

			y += si.iCharHeight;
			
			std::ostringstream out;
			out.setf(std::ios::fixed);
			out.precision(precision);
			out << "X: " << player.velocity[0] << "\n"
				<< "Y: " << player.velocity[1] << "\n"
				<< "Z: " << player.velocity[2] << "\n"
				<< "XY: " << length(player.velocity[0], player.velocity[1]) << "\n"
				<< "XYZ: " << length(player.velocity[0], player.velocity[1], player.velocity[2]);

			DrawMultilineString(x, y, out.str());
		}

		if (y_bxt_hud_origin.GetBool())
		{
			int x = -200,
				y = (si.iCharHeight * 6) + 1;
			if (!y_bxt_hud_origin_pos.IsEmpty())
			{
				std::istringstream pos_ss(y_bxt_hud_origin_pos.GetString());
				pos_ss >> x >> y;
			}

			x += si.iWidth;

			if (receivedAccurateInfo)
				DrawString(x, y, "Origin:");
			else
				DrawString(x, y, "Origin:", 1.0f, 0.0f, 0.0f);

			y += si.iCharHeight;

			std::ostringstream out;
			out.setf(std::ios::fixed);
			out.precision(precision);
			out << "X: " << player.origin[0] << "\n"
				<< "Y: " << player.origin[1] << "\n"
				<< "Z: " << player.origin[2];

			DrawMultilineString(x, y, out.str());
		}

		if (y_bxt_hud_speedometer.GetBool())
		{
			int x = 0,
				y = -2 * NumberHeight;
			if (!y_bxt_hud_speedometer_pos.IsEmpty())
			{
				std::istringstream pos_ss(y_bxt_hud_speedometer_pos.GetString());
				pos_ss >> x >> y;
			}

			x += si.iWidth / 2;
			y += si.iHeight;

			DrawNumber(static_cast<int>(trunc(length(player.velocity[0], player.velocity[1]))), x, y);
		}

		if (y_bxt_hud_jumpspeed.GetBool())
		{
			static float fadeEndTime = 0.0f;
			static int fadingFrom[3] = { hudColor[0], hudColor[1], hudColor[2] };
			static double jumpSpeed = length(player.velocity[0], player.velocity[1]);

			int r = hudColor[0],
				g = hudColor[1],
				b = hudColor[2];

			if (FADE_DURATION_JUMPSPEED > 0.0f)
			{
				if ((player.velocity[2] != 0.0f && prevVel[2] == 0.0f)
					|| (player.velocity[2] > 0.0f && prevVel[2] < 0.0f))
				{
					double difference = length(player.velocity[0], player.velocity[1]) - jumpSpeed;
					if (difference != 0.0f)
					{
						if (difference > 0.0f)
							vecCopy({ 0, 255, 0 }, fadingFrom);
						else
							vecCopy({ 255, 0, 0 }, fadingFrom);

						fadeEndTime = flTime + FADE_DURATION_JUMPSPEED;
						jumpSpeed = length(player.velocity[0], player.velocity[1]);
					}
				}

				double passedTime = flTime - fadeEndTime + FADE_DURATION_JUMPSPEED;
				if (passedTime <= 0.0f)
					passedTime = 0.0f;
				else if (passedTime > FADE_DURATION_JUMPSPEED || !std::isnormal(passedTime)) // Check for Inf, NaN, etc.
					passedTime = FADE_DURATION_JUMPSPEED;

				float colorVel[3] = { hudColor[0] - fadingFrom[0] / FADE_DURATION_JUMPSPEED,
				                      hudColor[1] - fadingFrom[1] / FADE_DURATION_JUMPSPEED,
				                      hudColor[2] - fadingFrom[2] / FADE_DURATION_JUMPSPEED };
				r = hudColor[0] - colorVel[0] * (FADE_DURATION_JUMPSPEED - passedTime);
				g = hudColor[1] - colorVel[1] * (FADE_DURATION_JUMPSPEED - passedTime);
				b = hudColor[2] - colorVel[2] * (FADE_DURATION_JUMPSPEED - passedTime);
			}

			int x = 0,
				y = -3 * NumberHeight;
			if (!y_bxt_hud_jumpspeed_pos.IsEmpty())
			{
				std::istringstream pos_ss(y_bxt_hud_jumpspeed_pos.GetString());
				pos_ss >> x >> y;
			}

			x += si.iWidth / 2;
			y += si.iHeight;

			DrawNumber(static_cast<int>(trunc(jumpSpeed)), x, y, r, g, b);
		}

		vecCopy(player.velocity, prevVel);
		receivedAccurateInfo = false;
	}

	void UpdatePlayerInfo(float vel[3], float org[3])
	{
		vecCopy(vel, player.velocity);
		vecCopy(org, player.origin);

		receivedAccurateInfo = true;
	}

	void UpdatePlayerInfoInaccurate(float vel[3], float org[3])
	{
		if (!receivedAccurateInfo)
		{
			vecCopy(vel, player.velocity);
			vecCopy(org, player.origin);
		}
	}
}

int CHudCustom_Wrapper::Init()
{
	CustomHud::Init();

	m_Initialized = true;
	m_iFlags = HUD_ACTIVE;
	clientDLL.AddHudElem(this);

	return 1;
}

int CHudCustom_Wrapper_NoVD::Init()
{
	CustomHud::Init();

	m_Initialized = true;
	m_iFlags = HUD_ACTIVE;
	clientDLL.AddHudElem(this);

	return 1;
}
