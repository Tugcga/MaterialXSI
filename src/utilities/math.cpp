float clamp(float value, float min, float max) {
	if (value < min) {
		return min;
	}

	if (value > max) {
		return max;
	}

	return value;
}

float math_min(float a, float b) {
	return a < b ? a : b;
}

float math_max(float a, float b) {
	return a > b ? a : b;
}