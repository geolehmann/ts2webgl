#define NOMINMAX
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <vector>
#include <string>

#include "HTMLheader.h"
#include <bitset>

#include "OpenFileDialog.h"
#include <omp.h>

struct Vertex
{
	double x, y, z;
	int32_t index;
	Vertex(){}
	Vertex(double _x, double _y, double _z) { x = _x; y = _y; z = _z; }
	Vertex(int32_t i, double _x, double _y, double _z) { index = i; x = _x; y = _y; z = _z; }
	bool operator!=(const Vertex &b) const { if (b.x != x && b.y != y && b.z != z) return true; return false; }
	float dist(Vertex b) { return sqrt(pow(x - b.x, 2) + pow(y - b.y, 2) + pow(z - b.z, 2)); }
};

struct Triangle
{
	int32_t index;
	int32_t v1, v2, v3;
	Triangle(int32_t i, int32_t _x, int32_t _y, int32_t _z) { index = i; v1 = _x; v2 = _y; v3 = _z; }
};

struct Color
{
	int r, g, b;
	int32_t hex;
	Color(){}
	Color(int _a, int _b, int _c){ r = _a; g = _b; b = _c; }
};

struct Segment
{
	int32_t seg1, seg2;
	Segment(int32_t _a, int32_t _b) { seg1 = _a; seg2 = _b; }
};

struct Marker
{
	int32_t md;
	std::string name;
	Marker(int32_t _md, std::string _name) { md = _md; name = _name; }
};

enum objtype { SURF, PTS, PLINE };
enum geotype {am1UK, SyOK, SyUK, WELL, LINEE, REF, UNDEF, am1REF, SyOKREF, SyUKREF};


struct GObj
{
	std::vector<Vertex> vertices;
	std::vector<Triangle> triangles;
	std::vector<Segment> segments;
	int32_t vertexnumber, trianglenumber;
	objtype type;
	geotype geo;
	Color color;
	std::string name;
	std::string description;
	std::vector<Marker> markers;
};

std::vector<GObj> objects;

struct BBox
{
	Vertex min, max;
};

Vertex min(const Vertex& a, const Vertex& b)
{
	return Vertex(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
}

Vertex max(const Vertex& a, const Vertex& b)
{
	return Vertex(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
}


BBox init_BBox(std::vector<Vertex>* nodes)
{
	BBox boundingbox;
	boundingbox.min = Vertex(FLT_MIN, FLT_MIN, FLT_MIN);
	boundingbox.max = Vertex(FLT_MAX, FLT_MAX, FLT_MAX);
	for (auto n : *nodes)
	{
		boundingbox.min = max(boundingbox.min, Vertex(n.x, n.y, n.z));
		boundingbox.max = min(boundingbox.max, Vertex(n.x, n.y, n.z));
		
	}
	return boundingbox;
}

std::string rgbToHex(Color rgb)
{
	const unsigned red = rgb.r, green = rgb.g, blue = rgb.b;
	char hexcol[16];
	snprintf(hexcol, sizeof hexcol, "%02x%02x%02x", red, green, blue);
	std::string ret(hexcol);
	return ret;
}


double _div = 1.0f; //optionale skalierung
std::vector<Vertex> allvertices;

enum WellType { NABO, FERN, BEMUST, KEIN };
bool flaechen = false, nabos = false, fernbos = false;

std::string updateWellNames(std::string name)
{
	std::string tmp;
	tmp = name;
	WellType bohrtyp;
	bohrtyp = KEIN;
	// BP_1042_2017, N609_1_2017            E1_37_2014, E1_1_1_2009, T1_4_3_2009, SAV_9_2014
	std::size_t nabo = name.find_first_of("N");
	std::size_t fern1 = name.find_first_of("E");
	std::size_t fern2 = name.find_first_of("S");
	std::size_t fern3 = name.find_first_of("T");
	std::size_t bemust = name.find_first_of("B");
	if (nabo != std::string::npos) bohrtyp = NABO;
	if (fern1 != std::string::npos) bohrtyp = FERN;
	if (fern2 != std::string::npos) bohrtyp = FERN;
	if (fern3 != std::string::npos) bohrtyp = FERN;
	if (bemust != std::string::npos) bohrtyp = BEMUST;

	if (bohrtyp == BEMUST)
	{
		std::size_t found = tmp.find_first_of("_");
		tmp[found] = ' ';
		found = tmp.find_first_of("_", found + 1);
		tmp[found] = '/';
	}
	if (bohrtyp == NABO)
	{
		nabos = true;
		std::size_t found = tmp.find_first_of("_");
		tmp[found] = char(32); // erstes _ auf jeden Fall durch Leerzeichen ersetzen
		found = tmp.find_first_of("_", found + 1);
		tmp[found] = '/';
	}

	if (bohrtyp == FERN)
	{
		fernbos = true;
		std::size_t found = tmp.find_first_of("_");
		tmp[found] = ' ';
		std::size_t found2 = tmp.find_first_of("_", found + 1);
		std::size_t found3 = tmp.find_first_of("_", found2 + 1);
		if (found2 != std::string::npos && found3 != std::string::npos)
		{
			tmp[found2] = '.';
			tmp[found3] = '/'; // x x.x/2009
		}
		if (found2 != std::string::npos && found3 == std::string::npos)
		{
			tmp[found2] = '/'; // x x/2009
		}
	}
	return tmp;
}

int main(int argc, char *argv[])
{
	std::cout << "SKUA-GOCAD 2015.5 Surface/Pointset/Curve/Wells to HTML-WEBGL Converter\n";
	std::cout << "============================================\n\n";
	std::cout << "(c) ZI-GEO Christian Lehmann 2017\n\n\n";

	openDialog();

	std::ifstream ifs(filename.c_str(), std::ifstream::in);
	if (!ifs.good())
	{
		std::cout << "Error loading:" << filename << " No file found!" << "\n";
		system("PAUSE");
		exit(0);
	}

	std::string line, key, rest;
	std::cout << "Started loading GOCAD ASCII file " << filename << "....\n";
	int linecounter = 0;
	
	while (!ifs.eof() && std::getline(ifs, line))
	{
		key = "";
		rest = "";
		std::stringstream stringstream(line);
		stringstream >> key >> std::ws >> rest >> std::ws; // erster teil der zeile in key, dann in rest


		if (key == "GOCAD" && rest == "VSet")
		{
			bool namefound = false;
			GObj new_object;
			new_object.type = PTS;
			new_object.geo = UNDEF;
			new_object.vertexnumber = 0;
			new_object.trianglenumber = 0;
			new_object.color = Color(0, 0, 0); // set standard color (black)
											   // jetzt bis Ende Surface durchgehen
			while (!ifs.eof())
			{
				std::getline(ifs, line);
				std::stringstream stringstream2(line);
				// nächste zeilen durchgehen
				key = "";
				rest = "";
				stringstream2 >> key >> std::ws >> rest >> std::ws;
				if (key == "END") break; // Loop der Schleife abbrechen, nächstes Objekt fängt an
				if (key == "name:" && !namefound)
				{
					new_object.name = rest;
					namefound = true;
					fprintf_s(stderr, "Loading object %s ...\n", rest.c_str());
					if (rest.find("am1") != std::string::npos && rest.find("UK") != std::string::npos) new_object.geo = am1UK;
					if (rest.find("Sy") != std::string::npos && rest.find("OK") != std::string::npos) new_object.geo = SyOK;
					if (rest.find("Sy") != std::string::npos && rest.find("UK") != std::string::npos) new_object.geo = SyUK;
				}
				if (key == "*atoms*color:")
				{
					float r = 0, g = 0, b = 0;
					std::string dummy;
					std::size_t found = rest.find("#");
					if (found != std::string::npos) // Farbe ist binär angegeben
					{
						if (rest[0] == '#') { rest.erase(0, 1); }
						std::string str;
						std::stringstream ss;
						int* rgb = new int[3];

						for (int i = 0; i < 3; i++) {
							str = rest.substr(i * 2, 2);
							ss << std::hex << str;
							ss >> rgb[i];
							ss.clear();
						}

						new_object.color = Color(rgb[0], rgb[1], rgb[2]);

					}
					else // Farbe ist als float angegeben
					{
						stringstream2 >> g >> std::ws >> b >> std::ws >> dummy; // dummy enthält die 1 am Ende der Farbzeile
						new_object.color = Color(std::stod(rest.c_str())*255.f, g*255.f, b*255.f);
					}
				}
				if (key == "VRTX")
				{
					std::string rw, hw, teufe;
					stringstream2 >> rw >> std::ws >> hw >> std::ws >> teufe >> std::ws;
					new_object.vertices.push_back(Vertex(new_object.vertexnumber, std::stod(rw)*_div, std::stod(hw)* _div, std::stod(teufe) * _div));
					allvertices.push_back(Vertex(new_object.vertexnumber, std::stod(rw)*_div, std::stod(hw)* _div, std::stod(teufe) * _div));
					new_object.vertexnumber = new_object.vertexnumber + 1;
				}

				if (key == "PVRTX")
				{
					std::string rw, hw, teufe, dummy3;
					stringstream2 >> rw >> std::ws >> hw >> std::ws >> teufe >> std::ws >> dummy3 >> std::ws;
					new_object.vertices.push_back(Vertex(new_object.vertexnumber, std::stod(rw)* _div, std::stod(hw) * _div, std::stod(teufe) * _div));
					allvertices.push_back(Vertex(new_object.vertexnumber, std::stod(rw)* _div, std::stod(hw) * _div, std::stod(teufe) * _div));
					new_object.vertexnumber = new_object.vertexnumber + 1;
				}
			}
			objects.push_back(new_object);
		}


		if (key == "GOCAD" && rest == "TSurf")
		{
			flaechen = true;
			bool namefound = false;
			GObj new_object;
			new_object.type = SURF;
			new_object.geo = UNDEF;
			new_object.vertexnumber = 0;
			new_object.trianglenumber = 0;
			new_object.color = Color(0, 0, 0); // set standard color (black)
			// jetzt bis Ende Surface durchgehen
			while (!ifs.eof())
			{
				std::getline(ifs, line);
				std::stringstream stringstream2(line);
				// nächste zeilen durchgehen
				key = "";
				rest = "";
				stringstream2 >> key >> std::ws >> rest >> std::ws;
				if (key == "END") break; // Loop der Schleife abbrechen, nächstes Objekt fängt an
				if (key == "name:" && !namefound)
				{
					new_object.name = rest;
					namefound = true;
					fprintf_s(stderr, "Loading object %s ...\n", rest.c_str());
					if (rest.find("am1") != std::string::npos && rest.find("UK") != std::string::npos) new_object.geo = am1UK;
					if (rest.find("Sy") != std::string::npos && rest.find("OK") != std::string::npos) new_object.geo = SyOK;
					if (rest.find("Sy") != std::string::npos && rest.find("UK") != std::string::npos) new_object.geo = SyUK;
				}
				if (key == "*solid*color:")
				{
					float r = 0, g = 0, b = 0;
					std::string dummy;
					std::size_t found = rest.find("#");
					if (found != std::string::npos) // Farbe ist binär angegeben
					{
						if (rest[0] == '#') { rest.erase(0, 1); }
						std::string str;
						std::stringstream ss;
						int* rgb = new int[3];

						for (int i = 0; i < 3; i++) {
							str = rest.substr(i * 2, 2);
							ss << std::hex << str;
							ss >> rgb[i];
							ss.clear();
						}

						new_object.color = Color(rgb[0], rgb[1], rgb[2]);

					}
					else // Farbe ist als float angegeben
					{
						stringstream2 >> g >> std::ws >> b >> std::ws >> dummy; // dummy enthält die 1 am Ende der Farbzeile
						new_object.color = Color(std::stod(rest.c_str())*255.f, g*255.f, b*255.f);
					}
				}
				if (key == "VRTX")
				{
					std::string rw, hw, teufe;
					stringstream2 >> rw >> std::ws >> hw >> std::ws >> teufe >> std::ws;
					new_object.vertices.push_back(Vertex(new_object.vertexnumber, std::stod(rw)*_div, std::stod(hw)* _div, std::stod(teufe) * _div));
					allvertices.push_back(Vertex(new_object.vertexnumber, std::stod(rw)*_div, std::stod(hw)* _div, std::stod(teufe) * _div));
					new_object.vertexnumber = new_object.vertexnumber + 1;
				}

				if (key == "PVRTX")
				{
					std::string rw, hw, teufe, dummy3;
					stringstream2 >> rw >> std::ws >> hw >> std::ws >> teufe >> std::ws >> dummy3 >> std::ws;
					new_object.vertices.push_back(Vertex(new_object.vertexnumber, std::stod(rw)* _div, std::stod(hw) * _div, std::stod(teufe) * _div));
					allvertices.push_back(Vertex(new_object.vertexnumber, std::stod(rw)* _div, std::stod(hw) * _div, std::stod(teufe) * _div));
					new_object.vertexnumber = new_object.vertexnumber + 1;
				}

				if (key == "TRGL")
				{
					int32_t a, b;
					stringstream2 >> a >> std::ws >> b >> std::ws;
					a--; b--;
					new_object.triangles.push_back(Triangle(new_object.trianglenumber, std::stoi(rest) - 1, a, b));
					new_object.trianglenumber = new_object.trianglenumber + 1;
				}
			}
			objects.push_back(new_object);
		}
		if (key == "GOCAD" && rest == "PLine")
		{
			bool namefound = false;
			GObj new_object;
			new_object.type = PLINE;
			new_object.vertexnumber = 0;
			new_object.trianglenumber = 0;
			new_object.color = Color(0, 0, 0); // gocad speichert bei ascii-lines manchmal keine color
			new_object.geo = UNDEF;

			while (!ifs.eof())
			{
				std::getline(ifs, line);
				std::stringstream stringstream(line);
				key = "";
				rest = "";
				stringstream >> key >> std::ws >> rest >> std::ws;
				if (key == "END") { break; }

				if (key == "name:" && !namefound)
				{
					new_object.name = rest;
					namefound = true;
					fprintf_s(stderr, "Loading object %s ...\n", rest.c_str());
					if (rest.find("am1") != std::string::npos) new_object.geo = am1UK;
					if (rest.find("Sy") != std::string::npos && rest.find("OK") != std::string::npos) new_object.geo = SyOK;
					if (rest.find("Sy") != std::string::npos && rest.find("UK") != std::string::npos) new_object.geo = SyUK;
					if (rest.find("Ref") != std::string::npos || rest.find("ref") != std::string::npos || rest.find("Rad") != std::string::npos || rest.find("rad") != std::string::npos) 
					{
						new_object.geo = REF; // Reflektor
						if (rest.find("am1") != std::string::npos) new_object.geo = am1REF;
						if (rest.find("Sy") != std::string::npos && rest.find("OK") != std::string::npos) new_object.geo = SyOKREF;
						if (rest.find("Sy") != std::string::npos && rest.find("UK") != std::string::npos) new_object.geo = SyUKREF;
					}
				}

				if (key == "*line*color:")
				{
					float r = 0, g = 0, b = 0;
					std::string dummy;
					std::size_t found = rest.find("#");
					if (found != std::string::npos) // Farbe ist binär angegeben
					{
						if (rest[0] == '#') { rest.erase(0, 1); }
						std::string str;
						std::stringstream ss;
						int* rgb = new int[3];

						for (int i = 0; i < 3; i++) {
							str = rest.substr(i * 2, 2);
							ss << std::hex << str;
							ss >> rgb[i];
							ss.clear();
						}

						new_object.color = Color(rgb[0], rgb[1], rgb[2]);

					}
					else // Farbe ist als float angegeben
					{
						stringstream >> g >> std::ws >> b >> std::ws >> dummy; // dummy enthält die 1 am Ende der Farbzeile
						new_object.color = Color(std::stod(rest.c_str())*255.f, g*255.f, b*255.f);
					}
				}

				if (key == "VRTX")
				{
					std::string rw, hw, teufe;
					stringstream >> rw >> std::ws >> hw >> std::ws >> teufe >> std::ws;
					double nteufe = std::stod(teufe);  if (nteufe > 0) nteufe = nteufe*-1;
					new_object.vertices.push_back(Vertex(std::stod(rw) * _div, std::stod(hw) * _div, nteufe * _div));
					allvertices.push_back(Vertex(std::stod(rw) * _div, std::stod(hw) * _div, nteufe * _div));
				}
				

				if (key == "PVRTX")
				{
					std::string rw, hw, teufe, dummy4;
					stringstream >> rw >> std::ws >> hw >> std::ws >> teufe >> std::ws >> dummy4 >> std::ws;
					double nteufe = std::stod(teufe);  if (nteufe > 0) nteufe = nteufe*-1;
					new_object.vertices.push_back(Vertex(std::stod(rw) * _div, std::stod(hw) * _div, nteufe * _div));
					allvertices.push_back(Vertex(std::stod(rw) * _div, std::stod(hw) * _div, nteufe * _div));
				}
				
				if (key == "SEG")
				{
					std::string seg1, seg2;
					seg1 = rest;
					stringstream >> seg2 >> std::ws;
					new_object.segments.push_back(Segment(std::stoi(seg1), std::stoi(seg2)));
					
				}
			}
			objects.push_back(new_object);
		}
			


		if (key == "GOCAD" && rest == "Well")
		{
			bool namefound = false;
			GObj new_object;
			new_object.type = PLINE;
			new_object.vertexnumber = 0;
			new_object.trianglenumber = 0;
			new_object.color = Color(255, 0, 0); // rot für Bohrungen
			new_object.geo = WELL;
			double refX = 0, refY = 0, refZ = 0; //horizontale Ablenkung
			while (!ifs.eof())
			{
				std::getline(ifs, line);
				std::stringstream stringstream2(line);
				key = "";
				rest = "";
				stringstream2 >> key >> std::ws >> rest >> std::ws;
				if (key == "END") break; // Loop der inneren Schleife abbrechen, nächstes Objekt fängt an
				if (key == "name:" && !namefound)
				{
					new_object.name = rest;
					namefound = true;
					fprintf_s(stderr, "Loading object %s ...\n", rest.c_str());
				}

				if (key == "WREF") // Bohransatzpunkt
				{
					std::string rw, hw, teufe;
					stringstream2 >> rw >> std::ws >> hw >> std::ws >> teufe >> std::ws;
					new_object.vertices.push_back(Vertex(new_object.vertexnumber, std::stod(rest)* _div, std::stod(rw)* _div, std::stod(hw)* _div));
					allvertices.push_back(Vertex(new_object.vertexnumber, std::stod(rest)* _div, std::stod(rw)* _div, std::stod(hw)* _div));
					refX = std::stod(rest);
					refY = std::stod(rw);
					refZ = std::stod(hw);
					new_object.vertexnumber = new_object.vertexnumber + 1;
				}

				if (key == "PATH") // Bohrlochvermessung
				{
					std::string rw, hw, teufe;
					stringstream2 >> rw >> std::ws >> hw >> std::ws >> teufe >> std::ws;
					new_object.vertices.push_back(Vertex(new_object.vertexnumber, (refX + std::stod(hw))* _div, (refY + std::stod(teufe))* _div, std::stod(rw)* _div));
					allvertices.push_back(Vertex(new_object.vertexnumber, (refX + std::stod(hw))* _div, (refY + std::stod(teufe))* _div, std::stod(rw)* _div));
					new_object.vertexnumber = new_object.vertexnumber + 1;
					
				}

				if (key == "MRKR") // Schichtgrenzenmarker
				{
					std::string dummy, md;
					stringstream2 >> dummy >> std::ws >> md >> std::ws;
					new_object.markers.push_back(Marker(std::stoi(md), rest));

				}

			}
			//generate SEGMENTS
			for (int i = 1; i < new_object.vertexnumber; i++)
			{
				if (i>2) new_object.segments.push_back(Segment(i, i+1)); // i>2 because otherwise first segment would be double 
			}
			objects.push_back(new_object);
		}

		linecounter++;
		if (linecounter == 10000) { std::cout << "Processed 10.000 lines\n"; linecounter = 0; }




	}
	if (objects.size() == 0) { fprintf_s(stderr, "\n\nError: No objects found, not a valid GOCAD ASCII file!\n\n"); system("PAUSE"); exit(0); }
	else
	{
		fprintf_s(stderr, "  ...done. \n\n");
		std::cout << "Number of GOCAD objects found:" << objects.size() << "\n\n";
	}

	for (int32_t i = 0; i < objects.size(); i++)
	{
		if (objects.at(i).type == PLINE) fprintf_s(stderr, "Line segment counter of object %s: %zu \n", objects.at(i).name.c_str(), objects.at(i).segments.size());
	}



	BBox box;
	box = init_BBox(&allvertices);
	Vertex box_middle = Vertex(box.max.x - box.min.x / 2, box.max.y - box.min.y / 2, 0); // small hack - ignore Z
	box_middle = Vertex(box.min.x, box.min.y, -500); //Koordinaten skalieren anhand Boundingbox

	// ==========================================================================================================================================
	//											jetzt der zweite teil - alles in html packen
	// ==========================================================================================================================================


	std::cout << "Started exporting to HTML...\n\n";
	std::ofstream html;
	std::string delimiter = ".";
	std::string file_firstname = filename.substr(0, filename.find(delimiter)); // dateiname ohne endung übernehmen
	html.open(file_firstname + ".html");
	std::string header(reinterpret_cast<const char*>(HTMLheader_txt), sizeof(HTMLheader_txt));
	html << header;


	std::string boxstring = "var box = new THREE.Vector2(); box.x=" + std::to_string(box_middle.x) + "; box.y=" + std::to_string(box_middle.y) + ";";
	html << boxstring;

	// update well Names
	std::string f_names_start = "function updateWellNames(){if (showWellNames==true){";
	html << f_names_start;
	for (int32_t i = 0; i<objects.size(); i++)
	{
		if (objects.at(i).geo == WELL)
		{
			std::string newname;
			newname = updateWellNames(objects.at(i).name);
			std::string sprites1 = "var spritey";
			std::string sprites2 = " = makeTextSprite('";
			std::string sprites3 = "',{ fontsize: 15, borderColor: { r: 0, g: 0, b: 0, a: 0.0 }, backgroundColor: { r: 0, g: 0, b: 0, a: 0.0 } });	spritey";
			std::string sprites4 = ".position.set(";
			std::string sprites4_2 = std::to_string(objects.at(i).vertices.at(objects.at(i).vertices.size() / 2).x - box_middle.x) + ", " + std::to_string(objects.at(i).vertices.at(objects.at(i).vertices.size() / 2).y - box_middle.y) + ", " + std::to_string(objects.at(i).vertices.at(objects.at(i).vertices.size() / 2).z - box_middle.z);
			std::string sprites4_3 = "); scene.add(spritey";
			std::string sprites5 = "); ";
			html << sprites1 << i << sprites2 << newname << sprites3 << i << sprites4 << sprites4_2 << sprites4_3 << i << sprites5;
			std::string t = "spritey" + std::to_string(i);			std::string t1 = ".translateX(-center_c.x);";			std::string t2 = ".translateY(-center_c.y);";			std::string t3 = ".translateZ(-center_c.z);";
			html << t << t1 << t << t2 << t << t3;
		}
	}
	std::string f_names_hide_start = "} if (showWellNames==false) {";
	html << f_names_hide_start;
	for (int32_t i = 0; i<objects.size(); i++)
	{
		if (objects.at(i).geo == WELL)
		{
			std::string sprites1 = "for (k = scene.children.length, k >= 0; k--;) {	if (scene.children[k].type == 'Sprite')scene.remove(scene.children[k]);}";
			html << sprites1;
		}
	}
	std::string f_names_hide_end = "}}";
	html << f_names_hide_end;

	// Javascript-Funktionen generieren
	for (int i = 0; i < objects.size(); i++)
	{
		std::string show = "function handleClick" + std::to_string(i) + "(cb) {	if (cb.checked == true) meshes[" + std::to_string(i) + "].visible = true;if (cb.checked == false) meshes[" + std::to_string(i) + "].visible = false;	}";
		html << show;
	}

	//------------------------------------------------ ON/OFF TOGGLES --------------------------------------------------------------------------
	char* toggles = R"=====(
function toggleAxes(cb) 
{	
if (cb.checked == true) 
{
axes.visible = true;

} 
if (cb.checked == false) 
{
axes.visible = false;
}
}

function toggleWellNames(cb) 
{	
if (cb.checked == true) 
{
showWellNames=true;
} 
if (cb.checked == false) 
{
showWellNames=false;
}
updateWellNames();
}

function toggleScale(cb) 
{	
if (cb.checked == true) 
{
scene.traverse(function (child) {
 if (child instanceof THREE.Line) {
            if (child.geometry.vertices.length == 2) {child.visible = true;   }
        }
 if (child instanceof THREE.Sprite) { child.visible = true; }
});
} 
if (cb.checked == false) 
{
scene.traverse(function (child) {
 if (child instanceof THREE.Line) {            if (child.geometry.vertices.length == 2) {child.visible = false;   }
        }
 if (child instanceof THREE.Sprite) { child.visible = false; }
});
}}

function toggleWireframe(cb) 
{	
if (cb.checked == true) 
{
    scene.traverse(function (child) {
        if (child instanceof THREE.Mesh) if (child.material.type == "MeshLambertMaterial") {
            child.material.wireframe = true;
        }
    });
} 
if (cb.checked == false) 
{
    scene.traverse(function (child) {
        if (child instanceof THREE.Mesh) if (child.material.wireframe == true) {
            child.material.wireframe = false;
        }
    });
}}

function toggleSurfaces(cb) 
{	
if (cb.checked == true) 
{
    scene.traverse(function (child) {
        if (child instanceof THREE.Mesh) if (child.material.type == "MeshLambertMaterial") {
            child.visible = true;
        }
    });
} 
if (cb.checked == false) 
{
    scene.traverse(function (child) {
        if (child instanceof THREE.Mesh) if (child.material.type == "MeshLambertMaterial") {
            child.visible = false;
        }
    });
}}

function toggleShader(cb) 
{	
if (cb.checked == true) 
{

} 
if (cb.checked == false) 
{

}
}

function toggleMarker(cb) 
{	
if (cb.checked == true) 
{
text5.style.visibility='visible';
    scene.traverse(function (child) {
        if (child instanceof THREE.Mesh) {
            if (child.geometry.type == "SphereBufferGeometry") child.visible = true;
        }
    });
} 
if (cb.checked == false) 
{
text5.style.visibility='hidden';
    scene.traverse(function (child) {
        if (child instanceof THREE.Mesh) {
            if (child.geometry.type == "SphereBufferGeometry") child.visible = false;
        }
    });
}
}

function toggleWells(cb) 
{	
if (cb.checked == true) 
{
    scene.traverse(function (child) {
        if (child instanceof THREE.Line) {
            if (child.material.color.r == 1 && child.material.color.g < 0.01 && child.material.color.b < 0.01 && child.material.color.g > 0.00 && child.material.color.b > 0.00) child.visible = true;
        }
    });
} 
if (cb.checked == false) 
{
    scene.traverse(function (child) {
        if (child instanceof THREE.Line) {
            if (child.material.color.r == 1 && child.material.color.g < 0.01 && child.material.color.b < 0.01 && child.material.color.g > 0.00 && child.material.color.b > 0.00) child.visible = false;
        }
    });
}
}

function toggleFern(cb) 
{	
if (cb.checked == true) 
{
    scene.traverse(function (child) {
        if (child instanceof THREE.Line) {
            if (child.material.color.r == 1 && child.material.color.g < 0.01 && child.material.color.b < 0.01 && child.material.color.g > 0.00 && child.material.color.b > 0.00 && child.name.indexOf('N') == -1) child.visible = true;
        }
    });
} 
if (cb.checked == false) 
{
    scene.traverse(function (child) {
        if (child instanceof THREE.Line) {
            if (child.material.color.r == 1 && child.material.color.g < 0.01 && child.material.color.b < 0.01 && child.material.color.g > 0.00 && child.material.color.b > 0.00 && child.name.indexOf('N') == -1) child.visible = false;
        }
    });
}
}

function toggleNabos(cb) 
{	
if (cb.checked == true) 
{
    scene.traverse(function (child) {
        if (child instanceof THREE.Line) {
            if (child.material.color.r == 1 && child.material.color.g < 0.01 && child.material.color.b < 0.01 && child.material.color.g > 0.00 && child.material.color.b > 0.00 && child.name.indexOf('N') > -1) child.visible = true;
        }
    });
} 
if (cb.checked == false) 
{
    scene.traverse(function (child) {
        if (child instanceof THREE.Line) {
            if (child.material.color.r == 1 && child.material.color.g < 0.01 && child.material.color.b < 0.01 && child.material.color.g > 0.00 && child.material.color.b > 0.00 && child.name.indexOf('N') > -1) child.visible = false;
        }
    });
}
}

)=====";
html << toggles;


char* legend_div = R"=====(
var text5 = document.createElement('div');
text5.style.position = 'absolute';
text5.style.width = 200;
text5.style.top = 10;
text5.align = 'right';
text5.style.right = 10;
text5.style.height = 900;
text5.style.visibility='hidden';
text5.innerHTML = "<b><u>Schichtgrenzen</u></b>\
<br><br>\
<i>A4</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#00FA9A; display: inline-block;'></div><br>\
<i>T4</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#808000; display: inline-block;'></div><br>\
<i>Na3_i</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#AFEEEE; display: inline-block;'></div><br>\
<i>Na3_h4</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#00ff86; display: inline-block;'></div><br>\
<i>am4</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#008080; display: inline-block;'></div><br>\
<i>Na3_h3</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#00FA9A; display: inline-block;'></div><br>\
<i>am3</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#008080; display: inline-block;'></div><br>\
<i>Na3_h2</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#00FA9A; display: inline-block;'></div><br>\
<i>am2</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#008080; display: inline-block;'></div><br>\
<i>Na3_h1</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#00FA9A; display: inline-block;'></div><br>\
<i>am1</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#008080; display: inline-block;'></div><br>\
<i>Na3&beta;2</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#87CEEB; display: inline-block;'></div><br>\
<i>K3RoSyOK</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#FFD700; display: inline-block;'></div><br>\
<i>K3RoSyUK</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#FFFF00; display: inline-block;'></div><br>\
<i>K3RoNa</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#0000FF; display: inline-block;'></div><br>\
<i>K3RoCt</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#FF0000; display: inline-block;'></div><br>\
<i>Na3&beta;1</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#7FFFD4; display: inline-block;'></div><br>\
<i>Na3&alpha;</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#00FFFF; display: inline-block;'></div><br>\
<i>A3</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#00FF00; display: inline-block;'></div><br>\
<i>T3</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#778899; display: inline-block;'></div><br>\
<i>A2r</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#7FFFD4; display: inline-block;'></div><br>\
<i>Na2r</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#00BFFF; display: inline-block;'></div><br>\
<i>K2Ct</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#DC143C; display: inline-block;'></div><br>\
<i>Na2</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#4169E1; display: inline-block;'></div><br>\
<i>A2</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#008B8B; display: inline-block;'></div><br>\
<i>Unbekannt</i> &nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#c0c0c0; display: inline-block;'></div><br>\
\
\
\ ";
text5.style.overflow = "hidden";
document.body.appendChild(text5);
</script>
)=====";
html << legend_div;


	
char* neu = R"=====(
<script>
var scene, camera, material, light, ambientLight, renderer;
var meshes = [];
)=====";
	html << neu;

	std::cout << "Started conversion to WEBGL mesh. This might take a while depending on object size and line segment counter.\n\n";
	#pragma omp parallel for
	for (int32_t i = 0; i < objects.size(); i++)
	{
			std::string data_1_surf_start = "var object" + std::to_string(i) + " = [ ";
			html << data_1_surf_start;
			std::string data;

			if (objects.at(i).type == SURF) data = std::to_string(objects.at(i).vertexnumber) + ", " + std::to_string(objects.at(i).trianglenumber) + ", "; // erst anzahl vertices, dann anzahl triangles
			if (objects.at(i).type == PLINE || objects.at(i).type == PTS) data = ""; // erst anzahl vertices, dann anzahl triangles
			int forcounter = 0, icounter=1;
			for (auto v : objects.at(i).vertices)
			{
				data = data + std::to_string(v.x - box_middle.x) + ", " + std::to_string(v.y - box_middle.y) + ", " + std::to_string(v.z - box_middle.z) + ", ";
				forcounter++;
				if (forcounter >= 10000) { fprintf_s(stderr, "Processed %lu segments out of %lu..\n", forcounter*icounter, objects.at(i).vertices.size()); forcounter = 0; icounter++; }
			}
			for (auto t : objects.at(i).triangles)data = data + std::to_string(t.v1) + ", " + std::to_string(t.v2) + ", " + std::to_string(t.v3) + ", ";

			if (data != "") data.pop_back();
			if (data != "") data.pop_back(); // letztes Komma + Leerzeichen löschen
			html << data;
			char* data_1_surf_end = " ];";
			html << data_1_surf_end; // definierte surfaces/lines sofort einspeisen. 
			fprintf_s(stderr, "Processed object %s (%u out of %u)...\n",objects.at(i).name.c_str(), i , objects.size());
	}

	// generate marker arrays


	for (int32_t i = 0; i < objects.size(); i++)
	{
		std::string markerlist;
		std::string markernames;
		int name_id; // -1 - am2, 0-am1, 1-Na3b2, 2-Sy, 3-Na3b1, 4 - K3RoCt, 5-A3, 6 - T3, 7 - K2Ct, 8- Na2, 9 - Na3_a
		if (objects.at(i).geo == WELL)
		{
			for (auto m : objects.at(i).markers)
			{
				markerlist = markerlist + std::to_string(m.md) + ", ";

				name_id = 666;
				if (m.name.find("A4") != std::string::npos) name_id = -10;
				if (m.name.find("T4") != std::string::npos) name_id = -9;
				if (m.name.find("Na3_i") != std::string::npos) name_id = -8;
				if (m.name.find("Na3_h4") != std::string::npos) name_id = -7;
				if (m.name.find("am4") != std::string::npos) name_id = -6;
				if (m.name.find("Na3_h3") != std::string::npos) name_id = -5;
				if (m.name.find("am3") != std::string::npos) name_id = -4;
				if (m.name.find("Na3_h2") != std::string::npos) name_id = -3;
				if (m.name.find("am2") != std::string::npos) name_id = -2;
				if (m.name.find("Na3_h1") != std::string::npos) name_id = -1;
				if (m.name.find("am1") != std::string::npos) name_id = 0;
				if (m.name.find("Na3_b2") != std::string::npos) name_id = 1;
				if (m.name.find("K3RoSyOK") != std::string::npos) name_id = 2;
				if (m.name.find("K3RoSyUK") != std::string::npos) name_id = 3;
				if (m.name.find("K3RoNa") != std::string::npos) name_id = 4;
				if (m.name.find("K3RoCt") != std::string::npos) name_id = 5;
				if (m.name.find("Na3_b1") != std::string::npos) name_id = 6;
				if (m.name.find("Na3_a") != std::string::npos) name_id = 7;
				if (m.name.find("A3") != std::string::npos) name_id = 8;
				if (m.name.find("T3") != std::string::npos) name_id = 9;
				if (m.name.find("A2r") != std::string::npos) name_id = 10;
				if (m.name.find("Na2r") != std::string::npos) name_id = 11;
				if (m.name.find("K2Ct") != std::string::npos) name_id = 12;
				if (m.name.find("Na2OK") != std::string::npos || m.name.find("Na2UK") != std::string::npos) name_id = 13;
				if (m.name.find("A2OK") != std::string::npos || m.name.find("A2UK") != std::string::npos) name_id = 14;

				markernames = markernames + std::to_string(name_id) + ", ";
			}
		}
		if (markerlist != "") { markerlist.pop_back(); markerlist.pop_back(); } // letztes Komma+Leerzeichen löschen
		if (markernames != "") { markernames.pop_back(); markernames.pop_back(); } // letztes Komma+Leerzeichen löschen
		html << "var marker" << i << " =[" << markerlist << "];";
		html << "var mrk_names" << i << " =[" << markernames << "];";
	}

	/*std::string namelist;
	for (int32_t i = 0; i < objects.size(); i++)
	{
		namelist = namelist + objects.at(i).name + ", ";
	}
	if (namelist != "") { namelist.pop_back(); namelist.pop_back(); } // letztes Komma+Leerzeichen löschen
	html << "var namelist =[" << namelist << "];";*/



	fprintf_s(stderr, "Finished converting mesh. Started generating HTML file...\n");

	// ======================= now LAST PART - FOOTER =====================================
	char* part2_start = R"=====(
init();
render();
function loadObjects()
{
var i = 0;
)====="; 
	html << part2_start;


	int32_t htmlcounter = 0;
		for (int i = 0; i < objects.size(); i++)
		{
			if (objects.at(i).geo == WELL) objects.at(i).name = updateWellNames(objects.at(i).name); //neu
			std::string assign_start;
			if (objects.at(i).type == SURF) 
			{
				// Surface
				assign_start = "loadTSurf('" + objects.at(i).name + "', ";
			}

			else if (objects.at(i).type == PTS)
			{
				// Surface
				assign_start = "loadVSet('" + objects.at(i).name + "', ";
			}
			else
			{
				// PLine
				char* indices_start = "indices=[";
				html << indices_start;
				std::string segparts;
				segparts = "";
				fprintf_s(stderr, "Processing segments..\n");
				#pragma omp parallel for
				for (auto &s : objects.at(i).segments) // jedes object hat x segments als paare, diese müssen in 'segparts' geschrieben werden
				{
					segparts = segparts + std::to_string(s.seg1-1) + std::string(", ") + std::to_string(s.seg2-1) + std::string(", ");
				}
				if (segparts != "") 
				{
					segparts.pop_back();
					segparts.pop_back();
				}
				html << segparts;
				char* indices_end = "];";
				html << indices_end;

				if (objects.at(i).geo == WELL) {
					assign_start = "loadWellSpline('" + objects.at(i).name + "', marker" + std::to_string(i) + ", mrk_names" + std::to_string(i) +", ";
				}
				else {
					assign_start = "loadPLine('" + objects.at(i).name + "', ";
				}
			}
			std::string name = "object" + std::to_string(i);
			std::string assign_end = "); ";
			html << assign_start << name << assign_end;

			std::string color_middle;
			char* color_start = R"=====(
			meshes[i++].material.color = new THREE.Color("rgb()=====";
			char* color_end = R"=====()");)=====";
			color_middle = std::to_string(objects.at(i).color.r) + std::string(", ") + std::to_string(objects.at(i).color.g) + std::string(", ") + std::to_string(objects.at(i).color.b);
			//if (ident.gtype.at(i) == UNDEF) color_middle = std::string("0") + std::string(", ") + std::string("0") + std::string(", ") + std::string("0");
			//if (objects.at(i).geo == am1UK || objects.at(i).geo == am1REF) color_middle = std::string("0") + std::string(", ") + std::string("255") + std::string(", ") + std::string("123");
			//if (objects.at(i).geo == SyUK || objects.at(i).geo == SyUKREF) color_middle = std::string("255") + std::string(", ") + std::string("255") + std::string(", ") + std::string("0");
			//if (objects.at(i).geo == SyOK || objects.at(i).geo == SyOKREF) color_middle = std::string("62") + std::string(", ") + std::string("186") + std::string(", ") + std::string("195");			
			if (objects.at(i).geo == WELL) color_middle = std::string("255") + std::string(", ") + std::string("1") + std::string(", ") + std::string("1");
			//if (objects.at(i).name.find("loez") != std::string::npos || objects.at(i).name.find("löz") != std::string::npos || objects.at(i).name.find("ager") != std::string::npos) color_middle = std::string("255") + std::string(", ") + std::string("255") + std::string(", ") + std::string("0");
			char* transparency = R"=====(
			meshes[i].material.transparent = true;		
			meshes[i].material.opacity = 0.6;
			)=====";
			if (objects.at(i).type == SURF && objects.at(i).type != SyOK&& objects.at(i).geo != UNDEF) html << transparency << color_start << color_middle << color_end;
			else html << color_start << color_middle << color_end;

			htmlcounter++;
			fprintf_s(stderr, "Done writing object Nr. %u.\n", htmlcounter);
		}

	// ==============================last part
	char* final_part1 = R"=====(
	addMeshes();
	};
	</script><canvas style="width: 1847px; height: 933px;" height="933" width="1847"></canvas>
  <div id="mySidenav" class="sidenav">
<a href="javascript:void(0)" class="closebtn" onclick="closeNav()">&times;</a>
    <span class="ksheader">&nbsp;&nbsp;&nbsp;K+S</span><br>
	<span class="ksheader2">&nbsp;&nbsp;&nbsp;GOCAD Betrachter</span><br>
	<span class="ksheader2">&nbsp;&nbsp;&nbsp;(c) ZI-GEO 2017</span><br>
<br>
    <div id="text1" class="tex1">&nbsp;</div>
    <div id="text4" class="tex4">&nbsp;</div>
<button class="accordion"><span style="color:#11a593;";>&#10876;</span>&nbsp;Navigation</button>
<div class="panel">
Kamerazentrum - 'A'<br>
Zoom - Mausrad<br>
Drehen - Linke Maustaste<br>
Verschieben - Rechte Maustaste
</div>
<button class="accordion"><span style="color:#11a593;";>&#10920;</span>&nbsp;Darstellung</button>
<div class="panel">
)=====";
	html << final_part1;


	// darstellung - je nach vorhandene objekte
	html << "<input type='checkbox' onclick='toggleAxes(this);'>Koordinatenachsen<br>";
		
	if (flaechen) html << "<input type='checkbox' onclick='toggleWireframe(this);'>Gitternetz<br>";
	if (flaechen) html << "<input type='checkbox' onclick='toggleSurfaces(this);' checked>Fl&auml;chen<br>";
	if (fernbos) html << "<input type='checkbox' onclick='toggleFern(this);' checked>Fernbohrungen<br>";
	if (fernbos) html << "<input type='checkbox' onclick='toggleMarker(this);'>Schichtgrenzen<br>";
	if (nabos) html << "<input type='checkbox' onclick='toggleNabos(this);' checked>Nabos";
	html << "</div>";


	

	// letzter teil - objekte
	char* final_part2 = R"=====(
  <button class="accordion"><span style="color:#11a593;";>&#9776;</span>&nbsp;Objekte</button>
<div class="panel">
	)=====";
	html << final_part2;

	// object definitions here
	std::string description;
	for (int i = 0; i < objects.size(); i++)
	{
		// feste Farben für features setzen
		//if (objects.at(i).geo == am1UK || objects.at(i).geo == am1REF) objects.at(i).color = Color(0, 255, 123);
		//if (objects.at(i).geo == SyUK || objects.at(i).geo == SyUKREF) objects.at(i).color = Color(255, 255, 0);
		//if (objects.at(i).geo == SyOK || objects.at(i).geo == SyOKREF) objects.at(i).color = Color(62, 186, 195);
		//if (objects.at(i).name.find("loez") != std::string::npos || objects.at(i).name.find("löz") != std::string::npos) objects.at(i).color = Color(255, 255, 0);

		if (objects.at(i).geo != WELL)
		{
			if (objects.at(i).type == SURF)
			{
				description = "<input type='checkbox' onclick='handleClick" + std::to_string(i) + "(this);' checked><span class='symbole';><span style='color:" + rgbToHex(objects.at(i).color) + "';>&#9652;</span></span>" + objects.at(i).name + "<br>";
			}
			if (objects.at(i).type == PLINE)
			{
				description = "<input type='checkbox' onclick='handleClick" + std::to_string(i) + "(this);' checked><span class='symbole';><span style='color:" + rgbToHex(objects.at(i).color) + "';>&#9472;</span></span>" + objects.at(i).name + "<br>";
			}

			if (objects.at(i).type == PTS)
			{
				description = "<input type='checkbox' onclick='handleClick" + std::to_string(i) + "(this);' checked><span class='symbole';><span style='color:" + rgbToHex(objects.at(i).color) + "';>&#8944;</span></span>" + objects.at(i).name + "<br>";
			}

			html << description << "\ ";
		}
	}
	html << "<br><br><br><br><br></div>";




	char* final_part3 = R"=====(
    		</div>
<br><br><br><br><br>
<div class="versteckt">
  <span id="butt" class="versteckt" style="font-size:30px;cursor:pointer;visibility:hidden;" onclick="openNav()">&nbsp;&#9776; </span>
</div>
  <script>
var acc = document.getElementsByClassName("accordion");
var i;

for (i = 0; i < acc.length; i++) {
  acc[i].onclick = function() {
    this.classList.toggle("active");
    var panel = this.nextElementSibling;
    if (panel.style.maxHeight){
      panel.style.maxHeight = null;
    } else {
      panel.style.maxHeight = panel.scrollHeight + "px";
    } 
  }
}
</script>
  <script>
  function openNav() {
      document.getElementById("mySidenav").style.width = "200px";
 document.getElementById("butt").style.visibility = "hidden";
      // document.body.style.backgroundColor = "rgba(0,0,0,0.4)";
  }
  function closeNav() {
    document.getElementById("text1").innerHTML ="";
    document.getElementById("text4").innerHTML ="";
      document.getElementById("mySidenav").style.width = "0";
 document.getElementById("butt").style.visibility = "visible";
      // document.body.style.backgroundColor = "white";
  }
  </script>
	</body></html>
)=====";
html << final_part3;



	html.close();
	fprintf_s(stderr, "\nDone writing to .html file.\n\n");
	//system("PAUSE");
	return 0;
}

