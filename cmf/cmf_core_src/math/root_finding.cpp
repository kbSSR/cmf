﻿
#include "root_finding.h"
#include <cmath>
#include <stdexcept>

bool use_bisection(bool last_time_bisection, double a, double b, double c, double d, double s, double tolerance)
{
	bool
		c1 = (s < (3 * a + b) * 0.25) || (s > b), // New value far out of initial boundaries
		c2 = last_time_bisection && (std::abs(s - b) >= (std::abs(b - c) * 0.5)),
		c3 = !last_time_bisection && (std::abs(s - b) >= (std::abs(c - d) * 0.5)),
		c4 = last_time_bisection && (std::abs(b - c) < tolerance),
		c5 = !last_time_bisection && (std::abs(c - d) < tolerance);
	return c1 || c2 || c3 || c4 || c5;

}

double cmf::math::BrentsMethod::operator()(double lower_bound, double upper_bound, double offset) const
{
	double a = lower_bound;
	double b = upper_bound;
	double fa = f(a) - offset;   // calculated now to save function calls
	double fb = f(b) - offset;   // calculated now to save function calls
	double fs = 0;      // initialize 

	if (!(fa * fb < 0))
	{
		throw std::runtime_error("Brent's method: Signs of f(lower_bound) and f(upper_bound) must be opposites");
	}

	if (std::abs(fa) < std::abs(b)) // if magnitude of f(lower_bound) is less than magnitude of f(upper_bound)
	{
		std::swap(a, b);
		std::swap(fa, fb);
	}

	double c = a;           // c now equals the largest magnitude of the lower and upper bounds
	double fc = fa;         // precompute function evalutation for point c by assigning it the same value as fa
	bool used_bisection = true;      // boolean flag used to evaluate if statement later on
	double s = 0;           // Our Root that will be returned
	double d = 0;           // Only used if mflag is unset (mflag == false)

	for (unsigned int iter = 1; iter < max_iterations; ++iter)
	{
		// stop if converged on root or error is less than tolerance
		if (std::abs(b - a) < tolerance) {
			return s;
		}

		// Check if inverse quadratic interpolation is ok
		if (fa != fc && fb != fc) {
			// use inverse quadratic interpolation
			s = (a * fb * fc / ((fa - fb) * (fa - fc)))
				+ (b * fa * fc / ((fb - fa) * (fb - fc)))
				+ (c * fa * fb / ((fc - fa) * (fc - fb)));
		}
		else {
			// use secant method
			s = b - fb * (b - a) / (fb - fa);
		}
		// Check cases for using bisection

		if (use_bisection(used_bisection, a, b, c, d, s, tolerance)) {
			// bisection method
			s = (a + b)*0.5;
			used_bisection = true;
		}
		else {
			used_bisection = false;
		}
		fs = f(s) - offset;  // calculate fs
		d = c;      // first time d is being used (wasnt used on first iteration because mflag was set)
		c = b;      // set c equal to upper bound
		fc = fb;    // set f(c) = f(b)

		if (fa * fs < 0) {  // fa and fs have opposite signs
			b = s;
			fb = fs;    // set f(b) = f(s)
		}
		else {
			a = s;
			fa = fs;    // set f(a) = f(s)
		}

		if (std::abs(fa) < std::abs(fb)) { // if magnitude of fa is less than magnitude of fb
			std::swap(a, b);     // swap a and b
			std::swap(fa, fb);   // make sure f(a) and f(b) are correct after swap
		}

	}

	throw std::runtime_error("Brent's method: The solution does not converge or iterations are not sufficient");
}

cmf::math::BrentsMethod::BrentsMethod(double _tolerance, unsigned int _max_iterations)
	: tolerance(_tolerance), max_iterations(_max_iterations)
{}