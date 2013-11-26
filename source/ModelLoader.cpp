#include "ModelLoader.h"

using namespace std;
using namespace boost::property_tree;


/**
 * Constructor.
 */
ModelLoader::ModelLoader() {
	mObjParser = new ObjParser();
}


/**
 * Deconstructor.
 */
ModelLoader::~ModelLoader() {
	delete mObjParser;
}


/**
 * Compute the bounding box of the model.
 * @param  {std::vector<GLfloat>} vertices Vertices of the model.
 * @return {std::vector<GLfloat>}          Vector with the minimum and maximum of the bounding box.
 */
vector<GLfloat> ModelLoader::computeBoundingBox( vector<GLfloat> vertices ) {
	// Index 0 - 2: bbMin
	// Index 3 - 5: bbMax
	vector<GLfloat> bb = vector<GLfloat>( 6 );

	for( int i = 0; i < vertices.size(); i += 3 ) {
		if( i == 0 ) {
			bb[0] = bb[3] = vertices[i];
			bb[1] = bb[4] = vertices[i + 1];
			bb[2] = bb[5] = vertices[i + 2];
			continue;
		}

		bb[0] = ( vertices[i] < bb[0] ) ? vertices[i] : bb[0];
		bb[1] = ( vertices[i + 1] < bb[1] ) ? vertices[i + 1] : bb[1];
		bb[2] = ( vertices[i + 2] < bb[2] ) ? vertices[i + 2] : bb[2];

		bb[3] = ( vertices[i] > bb[3] ) ? vertices[i] : bb[3];
		bb[4] = ( vertices[i + 1] > bb[4] ) ? vertices[i + 1] : bb[4];
		bb[5] = ( vertices[i + 2] > bb[5] ) ? vertices[i + 2] : bb[5];
	}

	return bb;
}


/**
 * Get the axis-aligned bounding box.
 * @return {std::vector<GLfloat>} Axis-aligned bounding box.
 */
vector<GLfloat> ModelLoader::getBoundingBox() {
	return mBoundingBox;
}


/**
 * Get the material of each face.
 * @return {std::vector<GLint>} The material index of each face.
 */
vector<GLint> ModelLoader::getFacesMtl() {
	return mObjParser->getFacesMtl();
}


/**
 * Get the faces.
 * @return {std::vector<GLuint>} Faces.
 */
vector<GLuint> ModelLoader::getFacesV() {
	return mObjParser->getFacesV();
}


/**
 * Get the loaded association of vertices to normals for each face.
 * @return {std::vector<GLuint>} The vertex normal indices of the faces.
 */
vector<GLuint> ModelLoader::getFacesVN() {
	return mObjParser->getFacesVN();
}


/**
 * Get the loaded association of vertices to texture coordinates for each face.
 * @return {std::vector<GLuint>} The vertex texture indices of the faces.
 */
vector<GLuint> ModelLoader::getFacesVT() {
	return mObjParser->getFacesVT();
}


/**
 * Get a list of the lights in the scene.
 * @return {std::vector<light_t>} Vector of the lights.
 */
vector<light_t> ModelLoader::getLights() {
	return mLights;
}


/**
 * Get the loaded materials of the MTL file.
 * @return {std::vector<material_t>} The materials.
 */
vector<material_t> ModelLoader::getMaterials() {
	return mObjParser->getMaterials();
}


/**
 * Get the vertex normals.
 * @return {std::vector<GLfloat>} Normals.
 */
vector<GLfloat> ModelLoader::getNormals() {
	return mObjParser->getNormals();
}


/**
 * Get the loaded vertex texture coordinates.
 * @return {std::vector<GLfloat>} The texture coordinates.
 */
vector<GLfloat> ModelLoader::getTextureCoordinates() {
	return mObjParser->getTextureCoordinates();
}


/**
 * Get the vertices.
 * @return {std::vector<GLfloat>} Vertices.
 */
vector<GLfloat> ModelLoader::getVertices() {
	return mObjParser->getVertices();
}


/**
 * Load (custom-defined) lights of the scene.
 * @param {std::string} filepath Path to the file.
 * @param {std::string} filename Name of the OBJ file it belongs to.
 */
void ModelLoader::loadLights( string filepath, string filename ) {
	string lightsfile = filepath + filename.replace( filename.rfind( ".obj" ), 4, ".light" );

	// Does lights file exist?
	ifstream f( lightsfile.c_str() );
	if( !f.good() ) {
		f.close();
		return;
	}
	f.close();

	ptree lightPropTree;
	json_parser::read_json( filepath + filename, lightPropTree );

	ptree::const_iterator end = lightPropTree.end();

	for( ptree::const_iterator iter = lightPropTree.begin(); iter != end; iter++ ) {
		light_t lightx;

		lightx.position[0] = (*iter).second.get<float>( "position.x" );
		lightx.position[1] = (*iter).second.get<float>( "position.y" );
		lightx.position[2] = (*iter).second.get<float>( "position.z" );
		lightx.position[3] = (*iter).second.get<float>( "position.w" );

		lightx.diffuse[0] = (*iter).second.get<float>( "diffuse.r" );
		lightx.diffuse[1] = (*iter).second.get<float>( "diffuse.g" );
		lightx.diffuse[2] = (*iter).second.get<float>( "diffuse.b" );
		lightx.diffuse[3] = (*iter).second.get<float>( "diffuse.a" );

		lightx.specular[0] = (*iter).second.get<float>( "specular.r" );
		lightx.specular[1] = (*iter).second.get<float>( "specular.g" );
		lightx.specular[2] = (*iter).second.get<float>( "specular.b" );
		lightx.specular[3] = (*iter).second.get<float>( "specular.a" );

		lightx.constantAttenuation = (*iter).second.get<float>( "attenuation.constant" );
		lightx.linearAttenuation = (*iter).second.get<float>( "attenuation.linear" );
		lightx.quadraticAttenuation = (*iter).second.get<float>( "attenuation.quadratic" );

		lightx.spotCutoff = (*iter).second.get<float>( "spotlight.cutoff" );
		lightx.spotExponent = (*iter).second.get<float>( "spotlight.exponent" );

		lightx.spotDirection[0] = (*iter).second.get<float>( "spotlight.direction.x" );
		lightx.spotDirection[1] = (*iter).second.get<float>( "spotlight.direction.y" );
		lightx.spotDirection[2] = (*iter).second.get<float>( "spotlight.direction.z" );

		mLights.push_back( lightx );
	}
}


/**
 * Load 3D model.
 * @param {std::string} filepath Path to the file, without file name.
 * @param {std::string} filename Name of the file.
 */
void ModelLoader::loadModel( string filepath, string filename ) {
	mObjParser->load( filepath, filename );
	vector<GLuint> facesV = mObjParser->getFacesV();
	vector<GLfloat> vertices = mObjParser->getVertices();
	mBoundingBox = this->computeBoundingBox( vertices );

	char msg[256];
	snprintf(
		msg, 256, "[ModelLoader] Imported model \"%s\". %lu vertices and %lu faces.",
		filename.c_str(), vertices.size() / 3, facesV.size() / 3
	);
	Logger::logInfo( msg );
}


/**
 * Load texture file.
 * @param  {std::string} filepath Path to the textures.
 * @return {GLuint}               ID of the created texture.
 */
GLuint ModelLoader::loadTexture( string filepath ) {
	Logger::logWarning( "[ModelLoader] loadTexture() not implemented yet, since moving away from Assimp" );
	return 0;

	// ilInit();

	// int textureIndex = 0;
	// aiString path;
	// aiReturn textureFound = material->GetTexture( aiTextureType_DIFFUSE, textureIndex, &path );

	// if( textureFound != AI_SUCCESS ) {
	// 	char msg[60];
	// 	snprintf( msg, 60, "[ModelLoader] No diffuse texture found in material %d.", materialIndex );
	// 	Logger::logDebug( msg );
	// 	throw 0;
	// }

	// // No need to load the file again, we already did that.
	// if( mFileToTextureID.count( path.data ) > 0 ) {
	// 	return mFileToTextureID[path.data];
	// }


	// ILuint imageID;
	// ilGenImages( 1, &imageID );

	// GLuint textureID;
	// glGenTextures( 1, &textureID );

	// ilBindImage( imageID );
	// ilEnable( IL_ORIGIN_SET );
	// ilOriginFunc( IL_ORIGIN_LOWER_LEFT );

	// string filename = filepath.append( path.data );
	// ILboolean success = ilLoadImage( (ILstring) filename.c_str() );

	// if( success ) {
	// 	ilConvertImage( IL_RGBA, IL_UNSIGNED_BYTE );

	// 	glBindTexture( GL_TEXTURE_2D, textureID );
	// 	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	// 	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	// 	glTexImage2D(
	// 		GL_TEXTURE_2D, 0, GL_RGBA,
	// 		ilGetInteger( IL_IMAGE_WIDTH ), ilGetInteger( IL_IMAGE_HEIGHT ), 0,
	// 		GL_RGBA, GL_UNSIGNED_BYTE, ilGetData()
	// 	);
	// 	glGenerateMipmap( GL_TEXTURE_2D );

	// 	glBindTexture( GL_TEXTURE_2D, 0 );
	// 	mFileToTextureID[path.data] = textureID;

	// 	Logger::logDebug( string( "[ModelLoader] Loaded texture " ).append( path.data ) );
	// }

	// ilDeleteImages( 1, &imageID );

	// if( !success ) {
	// 	Logger::logError( string( "[ModelLoader] Failed to load texture file " ).append( path.data ) );
	// 	throw 1;
	// }

	// return textureID;
}
