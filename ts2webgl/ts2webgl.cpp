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


struct Vertex
{
	int32_t index;
	double x, y, z;
	Vertex(){}
	Vertex(double _x, double _y, double _z) { x = _x; y = _y; z = _z; }
	Vertex(int32_t i, double _x, double _y, double _z) { index = i; x = _x; y = _y; z = _z; }
	bool operator!=(const Vertex &b) const { 

		if (b.x != x && b.y != y && b.z != z) return true;
		return false;
		
	}
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
enum geotype {am1UK, SyOK, SyUK, WELL, LINEE, REF, UNDEF};


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
	bool isClosed = false;
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

Vertex minCPU(const Vertex& a, const Vertex& b)
{
	return Vertex(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
}

Vertex maxCPU(const Vertex& a, const Vertex& b)
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
		boundingbox.min = maxCPU(boundingbox.min, Vertex(n.x, n.y, n.z));
		boundingbox.max = minCPU(boundingbox.max, Vertex(n.x, n.y, n.z));
		
	}
	return boundingbox;
}

double _div = 1.0f;
std::vector<Vertex> allvertices;

int main(int argc, char *argv[])
{
	std::cout << "SKUA-GOCAD 2015.5 Surface/Pointset/PLine/Wells to HTML-WEBGL Converter\n";
	std::cout << "============================================\n\n";
	std::cout << "(c) Christian Lehmann 05.05.2017\n\n\n";

	openDialog();

	//filename = "testneu2";

	std::ifstream ifs(filename.c_str(), std::ifstream::in);
	if (!ifs.good())
	{
		std::cout << "Error loading:" << filename << "; File not found!" << "\n";
		system("PAUSE");
		exit(0);
	}

	std::string line, key, rest;
	std::cout << "Started loading GOCAD ASCII file " << filename << "....";
	int linecounter = 0;

	Color lastcolor;
	
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
			while (!ifs.eof() /*&& std::getline(ifs, line)*/)
			{
				std::getline(ifs, line);
				std::stringstream stringstream2(line);
				// nächste zeilen durchgehen
				key = "";
				rest = "";
				stringstream2 >> key >> std::ws >> rest >> std::ws;
				if (key == "END") break; // Loop der inneren Schleife abbrechen, nächstes Objekt fängt an
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
					if (rest.find("Ref") != std::string::npos || rest.find("ref") != std::string::npos || rest.find("Rad") != std::string::npos || rest.find("rad") != std::string::npos) new_object.geo = REF; // Reflektor
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
			new_object.color = Color(255, 0, 0); // red
			new_object.geo = WELL;
			// jetzt bis Ende Surface durchgehen
			double refX = 0, refY = 0, refZ = 0;
			while (!ifs.eof() /*&& std::getline(ifs, line)*/)
			{
				std::getline(ifs, line);
				std::stringstream stringstream2(line);
				// nächste zeilen durchgehen
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

				if (key == "PATH") // Bhrmessung
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
		if (linecounter == 100000) { std::cout << "Processed 100.000 lines\n"; linecounter = 0; }




	}
	if (objects.size() == 0) { fprintf_s(stderr, "\n\nError: Not a valid GOCAD ASCII file!\n\n"); system("PAUSE"); exit(0); }
	else
	{
		fprintf_s(stderr, "  ...done. \n\n");
		std::cout << "Number of GOCAD objects found:" << objects.size() << "\n\n";
	}

		std::cout << "Started conversion to WEBGL mesh. This might take a while depending on mesh size and number of objects.\n\n";
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
	std::string file_firstname = filename.substr(0, filename.find(delimiter)); // dateiname ohne endung
	html.open(file_firstname + ".html");
	std::string header(reinterpret_cast<const char*>(HTMLheader), sizeof(HTMLheader));
	html << header;



	char* optional = R"=====(
text2.innerHTML = "<img height='42' width='42' src='https://pbs.twimg.com/profile_images/366226936/twitter-logo-kpluss_400x400.jpg'></img>\
<br>\
<b><u>Legende</u></b>\
<br>\
Anhydritmittel (<i>am1</i>)&nbsp;&nbsp;<div style='width:10px;height:5px;border:1px; background-color:green; display: inline-block;'></div><br>\
Fl&ouml;z Ronnenberg (<i>K3RoSy</i>) OK&nbsp;&nbsp;<div style='width:10px;height:5px;border:1px; background-color:red; display: inline-block;'></div><br>\
Fl&ouml;z Ronnenberg (<i>K3RoSy</i>) UK&nbsp;&nbsp;<div style='width:10px;height:5px;border:1px; background-color:yellow; display: inline-block;'></div><br>\
<br>";
)=====";
	html << optional;


	// todo - add text for each TSurf a new line
	// same for wells, for reflectors


	

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

			data.pop_back();
			data.pop_back(); // letztes Komma + Leerzeichen löschen
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
			//color_middle = std::to_string(objects.at(i).color.r) + std::string(", ") + std::to_string(objects.at(i).color.g) + std::string(", ") + std::to_string(objects.at(i).color.b);
			color_middle = std::string("0") + std::string(", ") + std::string("0") + std::string(", ") + std::string("0");
			if (ident.gtype.at(i) == UNDEF) color_middle = std::string("100") + std::string(", ") + std::string("0") + std::string(", ") + std::string("0");
			if (ident.gtype.at(i) == am1UK) color_middle = std::string("0") + std::string(", ") + std::string("255") + std::string(", ") + std::string("123");
			if (ident.gtype.at(i) == SyUK) color_middle = std::string("255") + std::string(", ") + std::string("255") + std::string(", ") + std::string("0");
			if (ident.gtype.at(i) == SyOK) color_middle = std::string("255") + std::string(", ") + std::string("124") + std::string(", ") + std::string("80");
			char* transparency = R"=====(
			meshes[i].material.transparent = true;		
			meshes[i].material.opacity = 0.6;
			)=====";
			if (ident.otype.at(i) == SURF && ident.gtype.at(i) != SyOK&& ident.gtype.at(i) != UNDEF) html << transparency << color_start << color_middle << color_end;
			else html << color_start << color_middle << color_end;
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

