#include "../src/JPutils/jp_hand_tracking.h"

#include <cmath>
#include <iostream>

namespace
{
	// Quaternion for a rotation of `degrees` about a unit axis.
	jp_hand::PhonePose rotated(float ax, float ay, float az, float degrees)
	{
		const float half = degrees * 3.14159265f / 360.0f;
		const float s = std::sin(half);
		return jp_hand::phonePose(ax * s, ay * s, az * s, std::cos(half),
			0.1f, 1.2f, -0.3f);
	}

	bool near(float a, float b)
	{
		return std::fabs(a - b) < 0.05f;
	}
}

int main()
{
	int failures = 0;
	auto check = [&](bool condition, const char *what) {
		if (!condition)
		{
			std::cerr << "FAIL: " << what << "\n";
			++failures;
		}
	};

	const auto level = rotated(0, 1, 0, 0);
	check(level.valid && near(level.tiltX, 0) && near(level.tiltY, 0) &&
		near(level.turn, 0), "identity is level");
	check(near(level.x, 0.1f) && near(level.y, 1.2f) && near(level.z, -0.3f),
		"position passes through");

	// ARKit: +X right, +Y up, camera looks down -Z.
	check(near(rotated(1, 0, 0, 30).tiltY, 30), "nose up about +X is positive tilt Y");
	check(near(rotated(0, 1, 0, -40).turn, 40), "turning right is positive");
	check(near(rotated(0, 0, 1, -20).tiltX, 20), "clockwise roll is positive tilt X");

	// Pitch does not leak into roll or turn, and the reverse.
	const auto pitched = rotated(1, 0, 0, 25);
	check(near(pitched.tiltX, 0) && near(pitched.turn, 0), "pitch isolated");
	const auto rolled = rotated(0, 0, 1, 35);
	check(near(rolled.tiltY, 0) && near(rolled.turn, 0), "roll isolated");

	// Unnormalised quaternions are normalised; garbage is rejected.
	const auto scaled = jp_hand::phonePose(0.2588f * 1.3f, 0, 0, 0.9659f * 1.3f, 0, 0, 0);
	check(scaled.valid && near(scaled.tiltY, 30), "normalised");
	check(!jp_hand::phonePose(0, 0, 0, 0, 0, 0, 0).valid, "zero quaternion rejected");
	check(!jp_hand::phonePose(NAN, 0, 0, 1, 0, 0, 0).valid, "NaN rejected");

	// Binding plumbing: a Pose binding samples the matching field.
	jp_hand::Snapshot snapshot;
	snapshot.captured = 10.0;
	snapshot.phone = rotated(1, 0, 0, 30);
	jp_hand::Binding binding;
	binding.source = {jp_hand::SourceKind::IPhoneLidar, {}, jp_hand::Detector::Pose};
	jp_hand::setSignal(binding, jp_hand::PhoneTiltY);
	check(binding.inputMin == -45 && binding.inputMax == 45, "tilt default range");
	const auto value = jp_hand::sample(snapshot, binding, 10.05);
	check(value.valid && near(value.raw, 30), "pose sample");
	jp_hand::setSignal(binding, jp_hand::PhoneY);
	check(near(jp_hand::sample(snapshot, binding, 10.05).raw, 1.2f), "height sample");
	snapshot.phone.valid = false;
	check(!jp_hand::sample(snapshot, binding, 10.05).valid, "no pose, no value");

	if (failures)
	{
		std::cerr << failures << " phone pose test(s) failed\n";
		return 1;
	}
	std::cout << "Phone pose tests passed\n";
	return 0;
}
