/*
 * Stickman Warfare SM0/SM1 model decompiler
 * Made by: VDavid003
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "zlib.h"

//#define VERBOSE
//Hopefully enough for everything
#define DECOMP_MAX_SIZE 1024*1024

//some type definitions
typedef struct Vector3 {
	float x;
	float y;
	float z;
} Vector3;

typedef struct PackedVector3 {
	uint16_t x;
	uint16_t y;
	uint16_t z;
} PackedVector3;

typedef struct PackedOjjektumVertexLegacy {
	PackedVector3 pos;
	uint16_t tu;
	uint16_t tv;
	uint8_t lu;
	uint8_t lv;
} PackedOjjektumVertexLegacy;

typedef struct PackedOjjektumVertex {
	PackedVector3 pos;
	uint16_t tu;
	uint16_t tv;
	float lu;
	float lv;
} PackedOjjektumVertex;

typedef struct Header {
	uint32_t textures_len;
	uint32_t indices_len;
	uint32_t vln;
	uint32_t indices_comp_len;
	uint32_t packedojjverts_comp_len;
	Vector3 scale;
} Header;

//Delphi string with max length of 50
typedef struct TexStr {
	uint8_t len;
	char str[50];
} TexStr;

//Which material applies to which face
typedef struct D3DXAttributeRange {
	uint32_t attribId;
	uint32_t faceStart;
	uint32_t faceCount;
	uint32_t vertexStart;
	uint32_t vertexCount;
} D3DXAttributeRange;

typedef struct UV {
	float u;
	float v;
} UV;

typedef struct Model {
	int vertices_len;
	Vector3* vertices;
	int indices_len;
	uint16_t* indices;
	int uvs_len;
	UV* uvs;
	int textures_len;
	char** textures;
	int texattribs_len;
	D3DXAttributeRange* texattribs;
} Model;

float unpack_float(uint16_t packed, float scale) {
	float unpack = packed;
	return ((unpack / 0xFFFF)*2-1)*scale;
}

//Convert Delphi style strings to C style string
char* TexStrToCSTR(TexStr texStr) {
	char* cstr = malloc(texStr.len + 1);

	//unsafe if length is faked to a larger number than 50 but meh
	memcpy(cstr, texStr.str, texStr.len);
	cstr[texStr.len] = '\0';
	return cstr;
}

void FreeModel(Model* model) {
	if(model->vertices)
		free(model->vertices);
	if(model->indices)
		free(model->indices);
	if(model->uvs)
		free(model->uvs);
	if(model->textures) {
		for(int i=0; i < model->textures_len; i++)
			free(model->textures[i]);
		free(model->textures);
	}
	if(model->texattribs)
		free(model->texattribs);
	free(model);
}

Model* LoadSMXModel(char* filename) {
	Model* model = malloc(sizeof(Model));
	int sm_version = 0;

	switch (filename[strlen(filename) - 1]) {
		case '0':
			printf("Treating file as SM0\n");
			sm_version = 0;
			break;
		case '1':
			printf("Treating file as SM1\n");
			sm_version = 1;
			break;
		default:
			printf("Couldn't get SM version number from filename. Exiting...\n");
			FreeModel(model);
			return NULL;
	}

	printf("Opening %s...\n", filename);

	FILE* fp = fopen(filename, "rb");
	if (fp == NULL) {
		printf("Could not open %s, exiting...\n", filename);
		return NULL;
	}

	fseek(fp, 0L, SEEK_END);
	int fsize = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	if (fsize < 0x20) {
		printf("Invalid file: file size smaller than sm0/sm1 header\n");
		fclose(fp);
		FreeModel(model);
		return NULL;
	}

	printf("Reading header...\n");
	Header header;
	fread(&header, sizeof(Header), 1, fp);
#ifdef VERBOSE
	printf("textures_len = %d\n", header.textures_len);
	printf("indices_len = %d\n", header.indices_len);
	printf("vln = %d\n", header.vln);
	printf("indices_comp_len = %d\n", header.indices_comp_len);
	printf("packedojjverts_comp_len = %d\n", header.packedojjverts_comp_len);
	printf("x = %f\n", header.scale.x);
	printf("y = %f\n", header.scale.x);
	printf("z = %f\n", header.scale.x);
#endif

	printf("Reading texture table and attr table...\n");
	model->texattribs_len = header.textures_len;
	model->textures_len = header.textures_len;
	TexStr* texStrs = malloc(header.textures_len * sizeof(TexStr));
	model->texattribs = malloc(header.textures_len * sizeof(D3DXAttributeRange));
	model->textures = malloc(header.textures_len * sizeof(char*));

	fread(texStrs, header.textures_len * sizeof(TexStr), 1, fp);
	fread(model->texattribs, header.textures_len * sizeof(D3DXAttributeRange), 1, fp);
	for (int i = 0; i < header.textures_len; i++) {
		model->textures[i] = TexStrToCSTR(texStrs[i]);
#ifdef VERBOSE
		printf("Texture %d: %s\n", i, model->textures[i]);
		printf("attribId: %d fs: %d fc: %d vs: %d vc: %d\n",
				model->texattribs[i].attribId,
				model->texattribs[i].faceStart,
				model->texattribs[i].faceCount,
				model->texattribs[i].vertexStart,
				model->texattribs[i].vertexCount);
#endif
	}
	free(texStrs);

	printf("Decompressing indices...\n");
	void* indices_comp = malloc(header.indices_comp_len);
	fread(indices_comp, header.indices_comp_len, 1, fp);
	off_t indices_len = DECOMP_MAX_SIZE;
	model->indices = malloc(indices_len);
	if (uncompress((uint8_t*)model->indices, &indices_len, indices_comp, header.indices_comp_len) != Z_OK) {
		printf("Decompression failed.\n");
		free(indices_comp);
		FreeModel(model);
		fclose(fp);
		return NULL;
	}
	model->indices_len = indices_len / 2 / 3;
	free(indices_comp);
#ifdef VERBOSE
	printf("Decompressed indices length: %d\n", indices_len);
#endif

	printf("Decompressing PackedOjjektumVertices...\n");
	void* packedojjverts_comp = malloc(header.packedojjverts_comp_len);
	fread(packedojjverts_comp, header.packedojjverts_comp_len, 1, fp);
	off_t packedojjverts_len = DECOMP_MAX_SIZE;
	void* packedojjverts = malloc(packedojjverts_len);
	if (uncompress(packedojjverts, &packedojjverts_len, packedojjverts_comp, header.packedojjverts_comp_len) != Z_OK) {
		printf("Decompression failed.\n");
		free(packedojjverts_comp);
		free(packedojjverts);
		FreeModel(model);
		fclose(fp);
		return NULL;
	}
	free(packedojjverts_comp);
#ifdef VERBOSE
	printf("Decompressed PackedOjjektumVertices length: %d\n", packedojjverts_len);
#endif

	printf("Extracting vertices and UVs from PackedOjjektumVertices...\n");
	if (sm_version == 0) {
		model->vertices_len = packedojjverts_len / sizeof(PackedOjjektumVertexLegacy);
		model->vertices = malloc(model->vertices_len * sizeof(Vector3));

		model->uvs_len = packedojjverts_len / sizeof(PackedOjjektumVertexLegacy);
		model->uvs = malloc(model->uvs_len * sizeof(UV));

		PackedOjjektumVertexLegacy* packedojjverts_sm = packedojjverts;
		for (int i = 0; i < model->vertices_len; i++) {
			model->vertices[i].x = unpack_float(packedojjverts_sm[i].pos.x, header.scale.x);
			model->vertices[i].y = unpack_float(packedojjverts_sm[i].pos.y, header.scale.y);
			model->vertices[i].z = -unpack_float(packedojjverts_sm[i].pos.z, header.scale.z);
			model->uvs[i].u = unpack_float(packedojjverts_sm[i].tu, 128.0f);
			model->uvs[i].v = unpack_float(packedojjverts_sm[i].tv, -128.0f);
		}
	} else {
		model->vertices_len = packedojjverts_len / sizeof(PackedOjjektumVertex);
		model->vertices = malloc(model->vertices_len * sizeof(Vector3));

		model->uvs_len = packedojjverts_len / sizeof(PackedOjjektumVertexLegacy);
		model->uvs = malloc(model->uvs_len * sizeof(UV));

		PackedOjjektumVertex* packedojjverts_sm = packedojjverts;
		for (int i = 0; i < model->vertices_len; i++) {
			model->vertices[i].x = unpack_float(packedojjverts_sm[i].pos.x, header.scale.x);
			model->vertices[i].y = unpack_float(packedojjverts_sm[i].pos.y, header.scale.y);
			model->vertices[i].z = -unpack_float(packedojjverts_sm[i].pos.z, header.scale.z);
			model->uvs[i].u = unpack_float(packedojjverts_sm[i].tu, 128.0f);
			model->uvs[i].v = unpack_float(packedojjverts_sm[i].tv, -128.0f);
		}
	}

	free(packedojjverts);
	fclose(fp);
	return model;
}

int WriteModelToObj(Model* model, char* filename) {
	char objFile[512];
	char mtlFile[512];
	sprintf(objFile, "%s.obj", filename);
	sprintf(mtlFile, "%s.mtl", filename);

	printf("Opening %s for writing...\n", objFile);
	FILE* fp = fopen(objFile, "w");

	fprintf(fp, "mtllib %s\n", mtlFile);

	printf("Writing %d vertices...\n", model->vertices_len);
	for (int i = 0; i < model->vertices_len; i++) {
		fprintf(fp, "v %.8e %.8e %.8e\n", model->vertices[i].x, model->vertices[i].y, model->vertices[i].z);
	}

	printf("Writing %d UVs...\n", model->uvs_len);
	for (int i = 0; i < model->uvs_len; i++) {
		fprintf(fp, "vt %.8e %.8e\n", model->uvs[i].u, model->uvs[i].v);
	}

	for (int i = 0; i < model->texattribs_len; i++) {
		printf("Writing %d faces with texture %s...\n", model->texattribs[i].faceCount, model->textures[model->texattribs[i].attribId]);
		fprintf(fp, "usemtl %s\n", model->textures[model->texattribs[i].attribId]);
		for (int j = model->texattribs[i].faceStart; j < model->texattribs[i].faceStart + model->texattribs[i].faceCount; j++) {
			//Flip the 3 vertices to invert normal, needed for some reason.
			fprintf(fp, "f %d/%d %d/%d %d/%d\n",
					model->indices[(j*3)+2]+1, model->indices[(j*3)+2]+1,
					model->indices[(j*3)+1]+1, model->indices[(j*3)+1]+1,
					model->indices[(j*3)+0]+1, model->indices[(j*3)+0]+1);
		}
	}

	printf("Closing %s...\n", objFile);
	fclose(fp);

	printf("Opening %s for writing...\n", mtlFile);
	fp = fopen(mtlFile, "w");
	for (int i = 0; i < model->texattribs_len; i++) {
		printf("Writing material for texture %s...\n", model->textures[model->texattribs[i].attribId]);
		fprintf(fp, "newmtl %s\n", model->textures[model->texattribs[i].attribId]);
		fprintf(fp, "map_Kd %s\n", model->textures[model->texattribs[i].attribId]);
	}
	printf("Closing %s...\n", mtlFile);
	fclose(fp);
}

int main(int argc, char *argv[]) {
	printf("Stickman Warfare SM0/SM1 model decompiler\n");
	printf("Made by: VDavid003\n\n");
	if (argc < 2) {
		printf("Usage: stickmodel_decomp <files>\n");
		return -1;
	}

	for(int i=0; i < argc-1; i++) {
		Model* model = LoadSMXModel(argv[i+1]);
		if (model) {
			WriteModelToObj(model, basename(argv[i+1]));
			FreeModel(model);
		}
		printf("\n");
	}

	return 0;
}
