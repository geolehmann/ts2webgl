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

struct identifier
{
	int32_t end_i = 0;
	std::vector<geotype> gtype;
	std::vector<objtype> otype;
	std::vector<Color> colors;
	std::vector<int32_t> old_i;
};

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
	std::cout << "(c) Christian Lehmann 15.05.2017\n\n\n";

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

	std::cout << "Started conversion to WEBGL mesh. This might take a while depending on object size and line segment counter.\n\n";
	identifier ident;
	BBox box;
	box = init_BBox(&allvertices);
	Vertex box_middle = Vertex(box.max.x - box.min.x / 2, box.max.y - box.min.y / 2, 0); // small hack - ignore Z
	box_middle = Vertex(box.min.x, box.min.y, -500);

	// ==========================================================================================================================================
	//											jetzt der zweite teil - alles in html packen
	// ==========================================================================================================================================

	std::ofstream html;
	std::string delimiter = ".";
	std::string file_firstname = filename.substr(0, filename.find(delimiter)); // dateiname ohne endung übernehmen
	html.open(file_firstname + ".html");
	std::string header(reinterpret_cast<const char*>(HTMLheader), sizeof(HTMLheader));
	html << header;


	// K+S - Logo ist als base64 eingebettet
	char* optional = R"=====(
text2.innerHTML = "<img height='42' width='42' src='data:image/jpeg;base64,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'></img>\
<br>\
<b><u>Legende</u></b>\
<br>\
Anhydritmittel (<i>am1</i>)&nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#00FF7B; display: inline-block;'></div><br>\
Fl&ouml;z Ronnenberg (<i>K3RoSy</i>) OK&nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#FFFF00; display: inline-block;'></div><br>\
Fl&ouml;z Ronnenberg (<i>K3RoSy</i>) UK&nbsp;&nbsp;<div style='width:20px;height:10px;border:1px; background-color:#FF7C50; display: inline-block;'></div><br>\
<br>\
<br><br><b><u>Objekte:</u></b><br>\
)=====";
	html << optional;


	// add text for each object
	std::string description;
	for (int i = 0; i < objects.size(); i++)
	{
		// feste Farben setzen
		if (objects.at(i).geo == am1UK || objects.at(i).geo == am1REF) objects.at(i).color = Color(0, 255, 123);
		if (objects.at(i).geo == SyUK || objects.at(i).geo == SyUKREF) objects.at(i).color = Color(255, 255, 0);
		if (objects.at(i).geo == SyOK || objects.at(i).geo == SyOKREF) objects.at(i).color = Color(255, 124, 80);
		description = objects.at(i).name + " " + "<div style='width:20px;height:10px;border:1px; background-color:" + rgbToHex(objects.at(i).color) + "; display: inline-block;'></div> <br>";
		html << description;
	}
	char* descr_end = R"=====(")====="; // abschließendes " - zeichen hinzufügen
	html << descr_end;
	fprintf_s(stderr, "...Finished writing legend.\n");
	

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
	



	std::string UNDEFline, UNDEFsurfvert, UNDEFsurftri;
	bool UNDEFsurffound = false, UNDEFlinefound = false, DEFfound=false;
	int32_t vertcounter = 0, tricounter = 0, undefcounter = 0;
	ident.end_i = -1;
	int32_t loopcounter = 0;

	#pragma omp parallel for // OMP
	for (int32_t i = 0; i < objects.size(); i++)
	{
		if (objects.at(i).geo != UNDEF)
		{
			DEFfound = true;
			std::string data_1_surf_start = "var object" + std::to_string(ident.end_i+1) + " = [ ";
			html << data_1_surf_start;
			std::string data;

			if (objects.at(i).type == SURF) data = std::to_string(objects.at(i).vertexnumber) + ", " + std::to_string(objects.at(i).trianglenumber) + ", "; // erst anzahl vertices, dann anzahl triangles
			if (objects.at(i).type == PLINE) data = ""; // erst anzahl vertices, dann anzahl triangles
			for (auto v : objects.at(i).vertices) data = data + std::to_string(v.x - box_middle.x) + ", " + std::to_string(v.y - box_middle.y) + ", " + std::to_string(v.z - box_middle.z) + ", ";
			for (auto t : objects.at(i).triangles)data = data + std::to_string(t.v1) + ", " + std::to_string(t.v2) + ", " + std::to_string(t.v3) + ", ";

			ident.otype.push_back(objects.at(i).type);
			ident.gtype.push_back(objects.at(i).geo);
			ident.colors.push_back(objects.at(i).color);
			ident.old_i.push_back(i);

			if (data != "") data.pop_back();
			if (data != "") data.pop_back(); // letztes Komma + Leerzeichen löschen
			html << data;
			char* data_1_surf_end = " ];";
			html << data_1_surf_end; // definierte surfaces/lines sofort einspeisen. 
			ident.end_i++;
		}
		if (objects.at(i).geo == UNDEF)
		{
			if (objects.at(i).type == PLINE)
			{
				UNDEFlinefound = true;
				for (int32_t ind=0; ind<objects.at(i).vertices.size();ind++) 
				{ 
					UNDEFline = UNDEFline + std::to_string(objects.at(i).vertices.at(ind).x - box_middle.x) + ", " + std::to_string(objects.at(i).vertices.at(ind).y - box_middle.y) + ", " + std::to_string(objects.at(i).vertices.at(ind).z - box_middle.z) + ", ";
				}
			}
			if (objects.at(i).type == SURF) 
			{
				UNDEFsurffound = true;
				vertcounter += objects.at(i).vertexnumber;
				tricounter += objects.at(i).trianglenumber;
				for (auto v : objects.at(i).vertices) UNDEFsurfvert = UNDEFsurfvert + std::to_string(v.x - box_middle.x) + ", " + std::to_string(v.y - box_middle.y) + ", " + std::to_string(v.z - box_middle.z) + ", ";
				for (auto t : objects.at(i).triangles) UNDEFsurftri = UNDEFsurftri + std::to_string(t.v1+undefcounter) + ", " + std::to_string(t.v2+undefcounter) + ", " + std::to_string(t.v3+undefcounter) + ", ";
				undefcounter = vertcounter;
			}
		}
		loopcounter++;
		if (loopcounter >= 10000)
		{
			fprintf_s(stderr, "Processed 10.000 objects...\n");
			loopcounter = 0;
		}
	}

	// undefinierte PLINES
	if (UNDEFlinefound)
	{
	//	if (!DEFfound) end_i++;
		std::string data_1_surf_start = "var object" + std::to_string(ident.end_i+1) + " = [ ";
		html << data_1_surf_start;
		UNDEFline.pop_back(); //
		UNDEFline.pop_back(); // letztes Komma + Leerzeichen löschen
		html << UNDEFline;
		char* data_1_surf_end = " ];";
		html << data_1_surf_end;
		//if (!DEFfound) end_i++;
		ident.otype.push_back(PLINE);
		ident.gtype.push_back(UNDEF);
		ident.colors.push_back(Color(0,0,0));
		ident.old_i.push_back(ident.end_i + 1);
	}

	// undefinierte SURFACES
	if (UNDEFsurffound)
	{
		if (!DEFfound) ident.end_i--;
		if (!UNDEFlinefound) ident.end_i--;
		if (!UNDEFlinefound && !DEFfound) ident.end_i=-2;
		std::string data_1_surf_start = "var object" + std::to_string(ident.end_i+2) + " = [ ";
		html << data_1_surf_start;
		std::string tmp = std::to_string(vertcounter) + ", " + std::to_string(tricounter) + ", "; //anfang ist vertex- und trianglenummer
		html << tmp;
		UNDEFsurftri.pop_back(); //
		UNDEFsurftri.pop_back(); // letztes Komma + Leerzeichen löschen
		html << UNDEFsurfvert;
		html << UNDEFsurftri;
		char* data_1_surf_end = " ];";
		html << data_1_surf_end;
		if (!DEFfound) ident.end_i++;
		ident.otype.push_back(SURF);
		ident.gtype.push_back(UNDEF);
		ident.colors.push_back(Color(0, 0, 0));
	}

	fprintf_s(stderr, "Finished converting mesh. Writing to HTML file...\n");


	// ======================= now LAST PART - FOOTER =====================================
	char* part2_start = R"=====(
init();
render();
function loadObjects()
{
var i = 0;
)====="; 
	html << part2_start;

	// collect segments of undefined meshes
	// convert segment indices - next object gets (segID-oldMaxSegID)
	std::vector <Segment> UNDEFsegments;
	int32_t oldMaxSegID = 0;
	int32_t currentMaxID = 0;
	for (auto &o : objects)
	{
		if (o.type == PLINE && o.geo == UNDEF)
		{
			for (auto &s : o.segments)
			{
				currentMaxID = std::max(currentMaxID, std::max(s.seg1, s.seg2));
				s.seg1 = s.seg1 + oldMaxSegID;
				s.seg2 = s.seg2 + oldMaxSegID;
				UNDEFsegments.push_back(s);
			}
			oldMaxSegID = currentMaxID;
		}
	}

	fprintf_s(stderr, "...Done converting indices. Started writing vertices and indices.\n");
	int32_t htmlcounter = 0;
		for (int i = 0; i < ident.otype.size(); i++)
		{
			char *assign_start;
			if (ident.otype.at(i) == SURF) {
				assign_start = R"=====(
			loadTSurf(
			)=====";
			}

			else 
			{
				// indices
				char* indices_start = R"=====(
				indices=[
				)=====";
				html << indices_start;
				std::string segparts;
				segparts = "";

				#pragma omp parallel for
				for (auto &s : objects.at(ident.old_i.at(i)).segments)
				{
					segparts = segparts + std::to_string(s.seg1-1) + std::string(", ") + std::to_string(s.seg2-1) + std::string(", ");
					if (ident.gtype.at(i) == UNDEF)
					{
						for (auto &sp : UNDEFsegments)
						{
							segparts = segparts + std::to_string(sp.seg1 - 1) + std::string(", ") + std::to_string(sp.seg2 - 1) + std::string(", ");
						}
					}
					fprintf_s(stderr, "Processing segments..\n");
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
			color_middle = std::to_string(ident.colors.at(i).r) + std::string(", ") + std::to_string(ident.colors.at(i).g) + std::string(", ") + std::to_string(ident.colors.at(i).b);
			//if (ident.gtype.at(i) == UNDEF) color_middle = std::string("0") + std::string(", ") + std::string("0") + std::string(", ") + std::string("0");
			if (ident.gtype.at(i) == am1UK || ident.gtype.at(i) == am1REF) color_middle = std::string("0") + std::string(", ") + std::string("255") + std::string(", ") + std::string("123");
			if (ident.gtype.at(i) == SyUK || ident.gtype.at(i) == SyUKREF) color_middle = std::string("255") + std::string(", ") + std::string("255") + std::string(", ") + std::string("0");
			if (ident.gtype.at(i) == SyOK || ident.gtype.at(i) == SyOKREF) color_middle = std::string("255") + std::string(", ") + std::string("124") + std::string(", ") + std::string("80");
			char* transparency = R"=====(
			meshes[i].material.transparent = true;		
			meshes[i].material.opacity = 0.6;
			)=====";
			if (ident.otype.at(i) == SURF && ident.gtype.at(i) != SyOK&& ident.gtype.at(i) != UNDEF) html << transparency << color_start << color_middle << color_end;
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

