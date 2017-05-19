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

int main(int argc, char *argv[])
{
	std::cout << "SKUA-GOCAD 2015.5 Surface/Pointset/PLine/Wells to HTML-WEBGL Converter\n";
	std::cout << "============================================\n\n";
	std::cout << "(c) ZI-GEO Christian Lehmann 15.05.2017\n\n\n";

	openDialog();

	//filename = "testneu2";

	std::ifstream ifs(filename.c_str(), std::ifstream::in);
	if (!ifs.good())
	{
		std::cout << "Error loading:" << filename << " No file found!" << "\n";
		system("PAUSE");
		exit(0);
	}

	std::string line, key, rest;
	std::cout << "Started loading GOCAD ASCII file " << filename << "....";
	int linecounter = 0;
	
	while (!ifs.eof() && std::getline(ifs, line))
	{
		key = "";
		rest = "";
		std::stringstream stringstream(line);
		stringstream >> key >> std::ws >> rest >> std::ws; // erster teil der zeile in key, dann in rest


		if (key == "GOCAD" && rest == "VSet")
		{
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
				if (key == "name:")
				{
					new_object.name = rest;
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
				if (key == "name:")
				{
					new_object.name = rest;
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
			GObj new_object;
			new_object.type = PLINE;
			new_object.vertexnumber = 0;
			new_object.trianglenumber = 0;
			new_object.color = Color(0, 0, 0); // gocad speichert bei ascii-lines keine color
			new_object.geo = UNDEF;

			while (!ifs.eof())
			{
				std::getline(ifs, line);
				std::stringstream stringstream(line);
				key = "";
				rest = "";
				stringstream >> key >> std::ws >> rest >> std::ws;
				if (key == "END") { break; }

				if (key == "name:")
				{
					new_object.name = rest;
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
				if (key == "name:")
				{
					new_object.name = rest;
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

			}
			//generate segments
			for (int i = 1; i < new_object.vertexnumber; i++)
			{
				new_object.segments.push_back(Segment(i, i+1));
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


	// Javascript-Funktionen generieren
	for (int i = 0; i < objects.size(); i++)
	{
		std::string show = "function handleClick" + std::to_string(i) + "(cb) {	if (cb.checked == true) meshes[" + std::to_string(i) + "].visible = true;if (cb.checked == false) meshes[" + std::to_string(i) + "].visible = false;	}";
		html << show;
	}

	char* shader = R"=====(
function toggleShader(cb) 
{	
if (cb.checked == true) 
{showShader=true;} 
if (cb.checked == false) 
{showShader=false;}}
)=====";
html << shader;

char* wellName = R"=====(
function toggleWellNames(cb) 
{	
if (cb.checked == true) 
{} 
if (cb.checked == false) 
{}}
)=====";
html << wellName;

char* wellScale = R"=====(
function toggleWellScale(cb) 
{	
if (cb.checked == true) 
{} 
if (cb.checked == false) 
{}}
)=====";
html << wellScale;




	// Beschriftungsbox 
	char* optional = R"=====(
text2.innerHTML = "<img height='42' width='42' src='data:image/png;base64,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'></img>\
<br>\
<b><u>Legende</u></b>\
<br><br>\
Anhydritmittel (<i>am1</i>)&nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#00FF7B; display: inline-block;'></div><br>\
Fl&ouml;z Ronnenberg (<i>K3RoSy</i>) OK&nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#3EBAC3; display: inline-block;'></div><br>\
Fl&ouml;z Ronnenberg (<i>K3RoSy</i>) UK&nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#FFFF00; display: inline-block;'></div><br>\
<br>\
<br><br><b><u>Objekte</u></b><br><br>\
)=====";
	html << optional;

	std::string description;
	for (int i = 0; i < objects.size(); i++)
	{
		// feste Farben für features setzen
		if (objects.at(i).geo == am1UK || objects.at(i).geo == am1REF) objects.at(i).color = Color(0, 255, 123);
		if (objects.at(i).geo == SyUK || objects.at(i).geo == SyUKREF) objects.at(i).color = Color(255, 255, 0);
		if (objects.at(i).geo == SyOK || objects.at(i).geo == SyOKREF) objects.at(i).color = Color(62, 186, 195);
		
		std::string switch_button_p1 = "<label class='topcoat-checkbox'><input type='checkbox' onclick='handleClick";
		std::string switch_button_p2 = "(this);' checked><div class='topcoat-checkbox__checkmark'></div>  </label>\ ";
		html << switch_button_p1 << i << switch_button_p2;

		if (objects.at(i).type == SURF)
		{
			description = " <img height='20' width='20' src=' data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADEAAAApAQMAAACm11OpAAAABlBMVEX///9wkr5unc09AAAAAXRSTlMAQObYZgAAAAFiS0dEAIgFHUgAAAAJcEhZcwAAAEgAAABIAEbJaz4AAABlSURBVBjTlcyxDcAgEENREAUlIzAKo8FoGYURKCkiHGFfpChV4uYVlr5zn+ebDIeMXaYh8zRPWcy6JKAM0J4GgMEIMJiAYTKYzQIwWIGlnIKm3zbmGIzbbg7maN5O5hh8e/8/dwFGR15S93ZRvAAAACV0RVh0ZGF0ZTpjcmVhdGUAMjAxNy0wNS0xOVQwNzowMzoxMS0wNDowMN5V65MAAAAldEVYdGRhdGU6bW9kaWZ5ADIwMTctMDUtMTlUMDc6MDM6MTEtMDQ6MDCvCFMvAAAAAElFTkSuQmCC'></img> " + objects.at(i).name + " <div style='width:20px;height:10px;border:1px; background-color:" + rgbToHex(objects.at(i).color) + "; display: inline-block;'></div> <br> ";
		}
		if (objects.at(i).type == PLINE)
		{
			description = " <img height='20' width='20' src=' data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADEAAAAwAQMAAACCDgD6AAAABlBMVEX///9wkr5unc09AAAAAXRSTlMAQObYZgAAAAFiS0dEAIgFHUgAAAAJcEhZcwAAAEgAAABIAEbJaz4AAAB7SURBVBjTfdChDYAwEIXhNhVIRugKIFFdiQkAyVgIwhx1WGQF4Ui4HxKawJnPvL7c1ZifaTEpblQL9JsaotoPF1aI72qZsvikCvGD1iz+tBJfWWbBGXlmOgyZfqB3elvQ77a3NmXFtz02LFzLjnJVeZFKD0OLxsXPLz4BhSUrjSOuFFcAAAAldEVYdGRhdGU6Y3JlYXRlADIwMTctMDUtMTlUMDc6MDI6MjMtMDQ6MDAoh5ZnAAAAJXRFWHRkYXRlOm1vZGlmeQAyMDE3LTA1LTE5VDA3OjAyOjIzLTA0OjAwWdou2wAAAABJRU5ErkJggg=='></img> " + objects.at(i).name + " <div style='width:20px;height:10px;border:1px; background-color:" + rgbToHex(objects.at(i).color) + "; display: inline-block;'></div> <br> ";
		}

		if (objects.at(i).type == PTS)
		{
			description = " <img height='20' width='20' src=' data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACgAAAApAQMAAAB9bPmLAAAABlBMVEX///9wkr5unc09AAAAAXRSTlMAQObYZgAAAAFiS0dEAIgFHUgAAAAJcEhZcwAAAEgAAABIAEbJaz4AAABiSURBVAjXY2BABXxgUh5M2jegk/JIaoCAGUyyg6X4DzB8AJGMP0Ak8x+QOIhkZgCRDAwgcSAAqmGQAbPsGNiBZD0DfwOIlD8AI+3AIjJgWSyA8QHY5g9gm3+gkxBxiBo8AADtFhio/JV8WgAAACV0RVh0ZGF0ZTpjcmVhdGUAMjAxNy0wNS0xOVQwNzowMjo0NS0wNDowMI04qtoAAAAldEVYdGRhdGU6bW9kaWZ5ADIwMTctMDUtMTlUMDc6MDI6NDUtMDQ6MDD8ZRJmAAAAAElFTkSuQmCC'></img> " + objects.at(i).name + " <div style='width:20px;height:10px;border:1px; background-color:" + rgbToHex(objects.at(i).color) + "; display: inline-block;'></div> <br> ";
		}


		html << description << "\\";
	}

	char* shaderbutton = R"=====(
<br><b><u>Darstellung</u></b>\
<br><br>\
		<label class='topcoat-checkbox'><input type='checkbox' onclick='toggleShader(this);' checked><div class='topcoat-checkbox__checkmark'></div> Shader-Effekte</label><br>\
		<label class='topcoat-checkbox'><input type='checkbox' onclick='toggleWellNames(this);' checked><div class='topcoat-checkbox__checkmark'></div> Bohrungsnamen</label><br>\
		<label class='topcoat-checkbox'><input type='checkbox' onclick='toggleWellScale(this);' checked><div class='topcoat-checkbox__checkmark'></div> Bohrskala</label><br>\
)=====";
	html << shaderbutton;

	char* descr_end = R"=====(";)====="; // abschließendes " - zeichen hinzufügen
	html << descr_end;

	

	char* neu = R"=====(
text2.style.top = 50 + 'px';
text2.style.left = 50 + 'px';
document.body.appendChild(text2);
</script>
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
			for (auto v : objects.at(i).vertices) data = data + std::to_string(v.x - box_middle.x) + ", " + std::to_string(v.y - box_middle.y) + ", " + std::to_string(v.z - box_middle.z) + ", ";
			for (auto t : objects.at(i).triangles)data = data + std::to_string(t.v1) + ", " + std::to_string(t.v2) + ", " + std::to_string(t.v3) + ", ";

			if (data != "") data.pop_back();
			if (data != "") data.pop_back(); // letztes Komma + Leerzeichen löschen
			html << data;
			char* data_1_surf_end = " ];";
			html << data_1_surf_end; // definierte surfaces/lines sofort einspeisen. 
			fprintf_s(stderr, "Processed %u out of %u objects...\n",i ,objects.size());
	}

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
			char *assign_start;
			if (objects.at(i).type == SURF) 
			{
				// Surface
				assign_start = R"=====(
			loadTSurf(
			)=====";
			}

			else if (objects.at(i).type == PTS)
			{
				// Surface
				assign_start = R"=====(
				loadVSet(
				)=====";
			}
			else
			{
				// PLine
				char* indices_start = R"=====(
				indices=[
				)=====";
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
				char* indices_end = R"=====(
				];
				)=====";
				html << indices_end;

				assign_start = R"=====(
				loadPLine(
				)=====";
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
			if (objects.at(i).geo == am1UK || objects.at(i).geo == am1REF) color_middle = std::string("0") + std::string(", ") + std::string("255") + std::string(", ") + std::string("123");
			if (objects.at(i).geo == SyUK || objects.at(i).geo == SyUKREF) color_middle = std::string("255") + std::string(", ") + std::string("255") + std::string(", ") + std::string("0");
			if (objects.at(i).geo == SyOK || objects.at(i).geo == SyOKREF) color_middle = std::string("62") + std::string(", ") + std::string("186") + std::string(", ") + std::string("195");
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
	char* final_part = R"=====(
	addMeshes();
	};
	</script><canvas style="width: 1847px; height: 933px;" height="933" width="1847"></canvas>
	</body></html>
	)====="; 
	html << final_part;

	html.close();
	fprintf_s(stderr, "\nDone writing to .html file.\n\n");
	//system("PAUSE");
	return 0;
}

