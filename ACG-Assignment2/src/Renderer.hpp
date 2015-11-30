#ifndef Renderer_H_included
#define Renderer_H_included

#include <vector>

#include "Sphere.hpp"
#include "Image.hpp"


class Renderer
{
protected:
	unsigned int mWidth;
	unsigned int mHeight;
	unsigned int mSamples;
	Color mClearColor;
	std::vector<Sphere> spheres;

public:
	Renderer() : mClearColor(0,1,1)
	{
	}

	void buildScene(const std::vector<Sphere>& spheres)
	{
		this->spheres = spheres;
	}

	Image render(unsigned int width, unsigned int height, unsigned int samples, Color clearColor = Color(0,1,1))
	{
		mClearColor = clearColor;

		mWidth = width;
		mHeight = height;
		mSamples = samples;

		/* Set camera origin and viewing direction (negative z direction) */
		Ray camera(Vector(50.0, 52.0, 295.6), Vector(0.0, -0.042612, -1.0).Normalized());

		/* Image edge vectors for pixel sampling */
		Vector cx = Vector(mWidth * 0.5135 / mHeight);
		Vector cy = (cx.Cross(camera.dir)).Normalized() * 0.5135;

		/* Final rendering */
		Image img(mWidth, mHeight);

		/* Loop over image rows */
		for (auto y = 0u; y < mHeight; y++)
		{
			std::cout << "\rRendering (" << mSamples * 4 << " spp) " << (100.0 * y / (mHeight - 1)) << "%     ";
			srand(y * y * y);

			/* Loop over row pixels */
			for (auto x = 0u; x < mWidth; x++)
			{
				img.setColor(x, y, Color());

				/* 2x2 subsampling per pixel */
				for (int sy = 0; sy < 2; sy++)
				{
					for (int sx = 0; sx < 2; sx++)
					{
						Color accumulated_radiance = Color();

						/* Compute radiance at subpixel using multiple samples */
						for (auto s = 0u; s < mSamples; s++)
						{
							const double r1 = 2.0 * drand48();
							const double r2 = 2.0 * drand48();

							/* Transform uniform into non-uniform filter samples */
							double dx;
							if (r1 < 1.0)
								dx = sqrt(r1) - 1.0;
							else
								dx = 1.0 - sqrt(2.0 - r1);

							double dy;
							if (r2 < 1.0)
								dy = sqrt(r2) - 1.0;
							else
								dy = 1.0 - sqrt(2.0 - r2);

							/* Ray direction into scene from camera through sample */
							Vector dir = cx * ((x + (sx + 0.5 + dx) / 2.0) / mWidth - 0.5) +
								cy * ((y + (sy + 0.5 + dy) / 2.0) / mHeight - 0.5) +
								camera.dir;

							/* Extend camera ray to start inside box */
							Vector start = camera.org + dir * 130.0;

							dir = dir.Normalized();

							/* Accumulate radiance */
							accumulated_radiance = accumulated_radiance +
								Radiance(Ray(start, dir), 0, 1) / mSamples;
						}

						accumulated_radiance = accumulated_radiance.clamp() * 0.25;

						img.addColor(x, y, accumulated_radiance);
					}
				}
			}
		}
		std::cout << std::endl;

		return img;
	}

private:
	/******************************************************************
	* Check for closest intersection of a ray with the scene;
	* returns true if intersection is found, as well as ray parameter
	* of intersection and id of intersected object
	*******************************************************************/
	bool Intersect(const Ray &ray, double &t, int &id)
	{
		const int n = spheres.size();
		t = 1e20;

		for (int i = 0; i < n; i++)
		{
			double d = spheres[i].Intersect(ray);
			if (d > 0.0  && d < t)
			{
				t = d;
				id = i;
			}
		}
		return t < 1e20;
	}


	/******************************************************************
	* Recursive path tracing for computing radiance via Monte-Carlo
	* integration; only considers perfectly diffuse, specular or
	* transparent materials;
	* after 5 bounces Russian Roulette is used to possibly terminate rays;
	* emitted light from light source only included on first direct hit
	* (possibly via specular reflection, refraction), controlled by
	* parameter E = 0/1;
	* on diffuse surfaces light sources are explicitely sampled;
	* for transparent objects, Schlick�s approximation is employed;
	* for first 3 bounces obtain reflected and refracted component,
	* afterwards one of the two is chosen randomly
	*******************************************************************/
	Color Radiance(const Ray &ray, int depth, int E)
	{
		depth++;

		int numSpheres = spheres.size();

		double t;
		int id = 0;

		if (!Intersect(ray, t, id))   /* No intersection with scene */
			return mClearColor;

		const Sphere &obj = spheres[id];

		Vector hitpoint = ray.org + ray.dir * t;    /* Intersection point */
		Vector normal = (hitpoint - obj.position).Normalized();  /* Normal at intersection */
		Vector nl = normal;

		/* Obtain flipped normal, if object hit from inside */
		if (normal.Dot(ray.dir) >= 0)
			nl = nl*-1.0;

		Color col = obj.color;

		/* Maximum RGB reflectivity for Russian Roulette */
		double p = col.Max();

		if (depth > 5 || !p)   /* After 5 bounces or if max reflectivity is zero */
		{
			if (drand48() < p)            /* Russian Roulette */
				col = col * (1 / p);        /* Scale estimator to remain unbiased */
			else
				return obj.emission * E;  /* No further bounces, only return potential emission */
		}

		if (obj.refl == DIFF)
		{
			/* Compute random reflection vector on hemisphere */
			double r1 = 2.0 * M_PI * drand48();
			double r2 = drand48();
			double r2s = sqrt(r2);

			/* Set up local orthogonal coordinate system u,v,w on surface */
			Vector w = nl;
			Vector u;

			if (fabs(w.x) > .1)
				u = Vector(0.0, 1.0, 0.0);
			else
				u = (Vector(1.0, 0.0, 0.0).Cross(w)).Normalized();

			Vector v = w.Cross(u);

			/* Random reflection vector d */
			Vector d = (u * cos(r1) * r2s +
				v * sin(r1) * r2s +
				w * sqrt(1 - r2)).Normalized();

			/* Explicit computation of direct lighting */
			Vector e;
			for (int i = 0; i < numSpheres; i++)
			{
				const Sphere &sphere = spheres[i];
				if (sphere.emission.x <= 0 && sphere.emission.y <= 0 && sphere.emission.z <= 0)
					continue; /* Skip objects that are not light sources */

				/* Randomly sample spherical light source from surface intersection */

				/* Set up local orthogonal coordinate system su,sv,sw towards light source */
				Vector sw = sphere.position - hitpoint;
				Vector su;

				if (fabs(sw.x) > 0.1)
					su = Vector(0.0, 1.0, 0.0);
				else
					su = Vector(1.0, 0.0, 0.0);

				su = (su.Cross(w)).Normalized();
				Vector sv = sw.Cross(su);

				/* Create random sample direction l towards spherical light source */
				double cos_a_max = sqrt(1.0 - sphere.radius * sphere.radius /
					(hitpoint - sphere.position).Dot(hitpoint - sphere.position));
				double eps1 = drand48();
				double eps2 = drand48();
				double cos_a = 1.0 - eps1 + eps1 * cos_a_max;
				double sin_a = sqrt(1.0 - cos_a * cos_a);
				double phi = 2.0*M_PI * eps2;
				Vector l = su * cos(phi) * sin_a +
					sv * sin(phi) * sin_a +
					sw * cos_a;
				l = l.Normalized();

				/* Shoot shadow ray, check if intersection is with light source */
				if (Intersect(Ray(hitpoint, l), t, id) && id == i)
				{
					double omega = 2 * M_PI * (1 - cos_a_max);

					/* Add diffusely reflected light from light source; note constant BRDF 1/PI */
					e = e + col.MultComponents(sphere.emission * l.Dot(nl) * omega) * M_1_PI;
				}
			}

			/* Return potential light emission, direct lighting, and indirect lighting (via
			recursive call for Monte-Carlo integration */
			return obj.emission * E + e + col.MultComponents(Radiance(Ray(hitpoint, d), depth, 0));
		}
		else if (obj.refl == SPEC)
		{
			/* Return light emission mirror reflection (via recursive call using perfect
			reflection vector) */
			return obj.emission +
				col.MultComponents(Radiance(Ray(hitpoint, ray.dir - normal * 2 * normal.Dot(ray.dir)),
				depth, 1));
		}

		/* Otherwise object transparent, i.e. assumed dielectric glass material */
		Ray reflRay(hitpoint, ray.dir - normal * 2 * normal.Dot(ray.dir));  /* Prefect reflection */
		bool into = normal.Dot(nl) > 0;       /* Bool for checking if ray from outside going in */
		double nc = 1;                        /* Index of refraction of air (approximately) */
		double nt = 1.5;                      /* Index of refraction of glass (approximately) */
		double nnt;

		if (into)      /* Set ratio depending on hit from inside or outside */
			nnt = nc / nt;
		else
			nnt = nt / nc;

		double ddn = ray.dir.Dot(nl);
		double cos2t = 1 - nnt * nnt * (1 - ddn*ddn);

		/* Check for total internal reflection, if so only reflect */
		if (cos2t < 0)
			return obj.emission + col.MultComponents(Radiance(reflRay, depth, 1));

		/* Otherwise reflection and/or refraction occurs */
		Vector tdir;

		/* Determine transmitted ray direction for refraction */
		if (into)
			tdir = (ray.dir * nnt - normal * (ddn * nnt + sqrt(cos2t))).Normalized();
		else
			tdir = (ray.dir * nnt + normal * (ddn * nnt + sqrt(cos2t))).Normalized();

		/* Determine R0 for Schlick�s approximation */
		double a = nt - nc;
		double b = nt + nc;
		double R0 = a*a / (b*b);

		/* Cosine of correct angle depending on outside/inside */
		double c;
		if (into)
			c = 1 + ddn;
		else
			c = 1 - tdir.Dot(normal);

		/* Compute Schlick�s approximation of Fresnel equation */
		double Re = R0 + (1 - R0) *c*c*c*c*c;   /* Reflectance */
		double Tr = 1 - Re;                     /* Transmittance */

		/* Probability for selecting reflectance or transmittance */
		double P = .25 + .5 * Re;
		double RP = Re / P;         /* Scaling factors for unbiased estimator */
		double TP = Tr / (1 - P);

		if (depth < 3)   /* Initially both reflection and trasmission */
			return obj.emission + col.MultComponents(Radiance(reflRay, depth, 1) * Re +
			Radiance(Ray(hitpoint, tdir), depth, 1) * Tr);
		else             /* Russian Roulette */
		if (drand48() < P)
			return obj.emission + col.MultComponents(Radiance(reflRay, depth, 1) * RP);
		else
			return obj.emission + col.MultComponents(Radiance(Ray(hitpoint, tdir), depth, 1) * TP);
	}
};

#endif