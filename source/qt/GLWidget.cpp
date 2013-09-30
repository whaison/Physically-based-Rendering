#include "GLWidget.h"

using namespace std;


/**
 * Constructor.
 * @param {QWidget*} parent Parent QWidget this QWidget is contained in.
 */
GLWidget::GLWidget( QWidget* parent ) : QGLWidget( parent ) {
	srand( (unsigned) time( 0 ) );

	CL* mCl = new CL();

	mModelMatrix = glm::mat4( 1.0f );
	mProjectionMatrix = glm::perspective(
		Cfg::get().value<float>( Cfg::PERS_FOV ),
		Cfg::get().value<float>( Cfg::WINDOW_WIDTH ) / Cfg::get().value<float>( Cfg::WINDOW_HEIGHT ),
		Cfg::get().value<float>( Cfg::PERS_ZNEAR ),
		Cfg::get().value<float>( Cfg::PERS_ZFAR )
	);

	mDoRendering = false;
	mFrameCount = 0;
	mPreviousTime = 0;
	mCamera = new Camera( this );
	mSampleCount = 0;
	mSelectedLight = -1;

	mTimer = new QTimer( this );
	connect( mTimer, SIGNAL( timeout() ), this, SLOT( update() ) );

	mTimeSinceStart = boost::posix_time::microsec_clock::local_time();

	this->startRendering();
}


/**
 * Destructor.
 */
GLWidget::~GLWidget() {
	this->stopRendering();
}


/**
 * Calculate the matrices for view, model, model-view-projection and normals.
 */
void GLWidget::calculateMatrices() {
	if( !mDoRendering ) {
		return;
	}

	mSampleCount = 0;

	mViewMatrix = glm::lookAt(
		mCamera->getEye_glmVec3(),
		mCamera->getAdjustedCenter_glmVec3(),
		mCamera->getUp_glmVec3()
	);

	// mModelMatrix = glm::mat4( 1.0f );

	// If no scaling is involved:
	mNormalMatrix = glm::mat3( mModelMatrix );
	// else:
	// mNormalMatrix = glm::inverseTranspose( glm::mat3( mModelMatrix ) );

	mModelViewProjectionMatrix = mProjectionMatrix * mViewMatrix * mModelMatrix;
}


/**
 * The camera has changed. Handle it.
 */
void GLWidget::cameraUpdate() {
	this->calculateMatrices();
}


/**
 * Delete data (buffers, textures) of the old model.
 */
void GLWidget::deleteOldModel() {
	// Delete old vertex array buffers
	if( mVA.size() > 0 ) {
		glDeleteBuffers( mVA.size(), &mVA[0] );
		glDeleteBuffers( 1, &mIndexBuffer );

		map<GLuint, GLuint>::iterator texIter = mTextureIDs.begin();
		while( texIter != mTextureIDs.end() ) {
			glDeleteTextures( 1, &((*texIter).second) );
			texIter++;
		}
	}
}


glm::vec3 GLWidget::getEyeRay( glm::mat4 matrix, float x, float y ) {
	glm::vec4 tmp = matrix * glm::vec4( x, y, 0.0f, 1.0f );
	glm::vec3 result( tmp[0] / tmp[3], tmp[1] / tmp[3], tmp[2] / tmp[3] );

	return result - mCamera->getEye_glmVec3();
}


/**
 * Initialize OpenGL and start rendering.
 */
void GLWidget::initializeGL() {
	glClearColor( 0.1f, 0.1f, 0.2f, 0.0f );

	glEnable( GL_DEPTH_TEST );
	glEnable( GL_MULTISAMPLE );

	glEnable( GL_ALPHA_TEST );
	glAlphaFunc( GL_ALWAYS, 0.0f );

	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glGenFramebuffers( 1, &mFramebuffer );

	mTargetTextures = vector<GLuint>( 2 );
	glGenTextures( 2, &mTargetTextures[0] );
	for( int i = 0; i < 2; i++ ) {
		glBindTexture( GL_TEXTURE_2D, mTargetTextures[i] );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL );
	}
	glBindTexture( GL_TEXTURE_2D, 0 );

	this->initShaders();

	Logger::logInfo( string( "[OpenGL] Version " ).append( (char*) glGetString( GL_VERSION ) ) );
	Logger::logInfo( string( "[OpenGL] GLSL " ).append( (char*) glGetString( GL_SHADING_LANGUAGE_VERSION ) ) );
}


/**
 * Load and compile the shader.
 */
void GLWidget::initShaders() {
	GLenum err = glewInit();

	if( err != GLEW_OK ) {
		Logger::logError( string( "[GLEW] Init failed: " ).append( (char*) glewGetErrorString( err ) ) );
		exit( 1 );
	}
	Logger::logInfo( string( "[GLEW] Version " ).append( (char*) glewGetString( GLEW_VERSION ) ) );

	mGLProgram = glCreateProgram();
	GLuint vertexShader = glCreateShader( GL_VERTEX_SHADER );
	GLuint fragmentShader = glCreateShader( GL_FRAGMENT_SHADER );

	string shaderPath = Cfg::get().value<string>( Cfg::SHADER_PATH );
	shaderPath.append( Cfg::get().value<string>( Cfg::SHADER_NAME ) );

	this->loadShader( vertexShader, shaderPath + string( ".vert" ) );
	this->loadShader( fragmentShader, shaderPath +string( ".frag" ) );

	glLinkProgram( mGLProgram );
	glUseProgram( mGLProgram );
}


/**
 * Check, if QGLWidget is currently rendering.
 * @return {bool} True, if is rendering, false otherwise.
 */
bool GLWidget::isRendering() {
	return ( mDoRendering && mTimer->isActive() );
}


/**
 * Load 3D model and start rendering it.
 * @param {string} filepath Path to the file, without file name.
 * @param {string} filename Name of the file.
 */
void GLWidget::loadModel( string filepath, string filename ) {
	this->deleteOldModel();

	ModelLoader* ml = new ModelLoader();

	mVA = ml->loadModel( filepath, filename );
	mIndexBuffer = ml->getIndexBuffer();
	mNumIndices = ml->getNumIndices();
	mTextureIDs = ml->getTextureIDs();
	mLights = ml->getLights();

	// Ready
	this->startRendering();
}


/**
 * Load a GLSL shader and attach it to the program.
 * @param {GLuint}     shader ID of the shader.
 * @param {std:string} path   File path and name.
 */
void GLWidget::loadShader( GLuint shader, string path ) {
	string shaderString = utils::loadFileAsString( path.c_str() );
	const GLchar* shaderSource = shaderString.c_str();
	const GLint shaderLength = shaderString.size();

	glShaderSource( shader, 1, &shaderSource, &shaderLength );
	glCompileShader( shader );

	GLint status;
	glGetShaderiv( shader, GL_COMPILE_STATUS, &status );

	if( status != GL_TRUE ) {
		char logBuffer[1000];
		glGetShaderInfoLog( shader, 1000, 0, logBuffer );
		Logger::logError( string( "[Shader]\n" ).append( logBuffer ) );
		exit( 1 );
	}

	glAttachShader( mGLProgram, shader );
}


/**
 * Set a minimum width and height for the QWidget.
 * @return {QSize} Minimum width and height.
 */
QSize GLWidget::minimumSizeHint() const {
	return QSize( 50, 50 );
}


/**
 * Move the camera or if selected a light.
 * @param {const int} key Key code.
 */
void GLWidget::moveCamera( const int key ) {
	if( !this->isRendering() ) {
		return;
	}

	switch( key ) {

		case Qt::Key_W:
			if( mSelectedLight == -1 ) {
				mCamera->cameraMoveForward();
			}
			else {
				mLights[mSelectedLight].position[0] += 0.5f;
			}
			break;

		case Qt::Key_S:
			if( mSelectedLight == -1 ) {
				mCamera->cameraMoveBackward();
			}
			else {
				mLights[mSelectedLight].position[0] -= 0.5f;
			}
			break;

		case Qt::Key_A:
			if( mSelectedLight == -1 ) {
				mCamera->cameraMoveLeft();
			}
			else {
				mLights[mSelectedLight].position[2] += 0.5f;
			}
			break;

		case Qt::Key_D:
			if( mSelectedLight == -1 ) {
				mCamera->cameraMoveRight();
			}
			else {
				mLights[mSelectedLight].position[2] -= 0.5f;
			}
			break;

		case Qt::Key_Q:
			if( mSelectedLight == -1 ) {
				mCamera->cameraMoveUp();
			}
			else {
				mLights[mSelectedLight].position[1] += 0.5f;
			}
			break;

		case Qt::Key_E:
			if( mSelectedLight == -1 ) {
				mCamera->cameraMoveDown();
			}
			else {
				mLights[mSelectedLight].position[1] -= 0.5f;
			}
			break;

		case Qt::Key_R:
			mCamera->cameraReset();
			break;

	}
}


/**
 * Draw the scene.
 */
void GLWidget::paintGL() {
	if( !mDoRendering ) {
		return;
	}

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );


	glUniformMatrix4fv(
		glGetUniformLocation( mGLProgram, "modelViewProjectionMatrix" ), 1, GL_FALSE, &mModelViewProjectionMatrix[0][0]
	);
	glUniformMatrix4fv(
		glGetUniformLocation( mGLProgram, "modelMatrix" ), 1, GL_FALSE, &mModelMatrix[0][0]
	);
	glUniformMatrix4fv(
		glGetUniformLocation( mGLProgram, "viewMatrix" ), 1, GL_FALSE, &mViewMatrix[0][0]
	);
	glUniformMatrix3fv(
		glGetUniformLocation( mGLProgram, "normalMatrix" ), 1, GL_FALSE, &mNormalMatrix[0][0]
	);
	glUniform3fv(
		glGetUniformLocation( mGLProgram, "eye" ), 3, &(mCamera->getEye())[0]
	);


	// Light(s)

	glUniform1i( glGetUniformLocation( mGLProgram, "numLights" ), mLights.size() );
	char lightName1[20];
	char lightName2[20];

	for( uint i = 0; i < mLights.size(); i++ ) {
		snprintf( lightName1, 20, "light%uData1", i );
		snprintf( lightName2, 20, "light%uData2", i );

		float lightData1[16] = {
			mLights[i].position[0], mLights[i].position[1], mLights[i].position[2], mLights[i].position[3],
			mLights[i].diffuse[0], mLights[i].diffuse[1], mLights[i].diffuse[2], mLights[i].diffuse[3],
			mLights[i].specular[0], mLights[i].specular[1], mLights[i].specular[2], mLights[i].specular[3],
			mLights[i].constantAttenuation, mLights[i].linearAttenuation, mLights[i].quadraticAttenuation, mLights[i].spotCutoff
		};
		float lightData2[4] = {
			mLights[i].spotExponent, mLights[i].spotDirection[0], mLights[i].spotDirection[1], mLights[i].spotDirection[2]
		};

		glUniformMatrix4fv( glGetUniformLocation( mGLProgram, lightName1 ), 1, GL_FALSE, &lightData1[0] );
		glUniform4fv( glGetUniformLocation( mGLProgram, lightName2 ), 1, &lightData2[0] );
	}


	boost::posix_time::time_duration msdiff = boost::posix_time::microsec_clock::local_time() - mTimeSinceStart;
	glUniform1f( glGetUniformLocation( mGLProgram, "timeSinceStart" ), msdiff.total_milliseconds() * 0.001 );


	glm::vec3 v = glm::vec3( rand() / (float) RAND_MAX * 2.0f - 1.0f, rand() / (float) RAND_MAX * 2.0f - 1.0f, 0.0f );
	glm::mat4 jitter = this->getJitterMatrix( v ) * ( 1.0f / 512.0f );

	glm::vec3 ray00 = this->getEyeRay( jitter, -1.0f, -1.0f );
	glm::vec3 ray01 = this->getEyeRay( jitter, -1.0f, +1.0f );
	glm::vec3 ray10 = this->getEyeRay( jitter, +1.0f, -1.0f );
	glm::vec3 ray11 = this->getEyeRay( jitter, +1.0f, +1.0f );
	glUniform3fv( glGetUniformLocation( mGLProgram, "ray00" ), 1, &ray00[0] );
	glUniform3fv( glGetUniformLocation( mGLProgram, "ray01" ), 1, &ray01[0] );
	glUniform3fv( glGetUniformLocation( mGLProgram, "ray10" ), 1, &ray10[0] );
	glUniform3fv( glGetUniformLocation( mGLProgram, "ray11" ), 1, &ray11[0] );

	glUniform1f( glGetUniformLocation( mGLProgram, "textureWeight" ), mSampleCount / (float) ( mSampleCount + 1 ) );

	reverse( mTargetTextures.begin(), mTargetTextures.end() );
	mSampleCount++;

	this->paintScene();
	this->showFPS();
}


glm::mat4 GLWidget::getJitterMatrix( glm::vec3 v ) {
	glm::mat4 jitter = glm::mat4( 1.0f );

	jitter[0][3] = v[0];
	jitter[1][3] = v[1];
	jitter[2][3] = v[2];

	return glm::inverse( jitter * mModelViewProjectionMatrix );
}


/**
 * Draw the main objects of the scene.
 */
void GLWidget::paintScene() {
	// for( GLuint i = 0; i < mVA.size(); i++ ) {
	// 	GLfloat useTexture = 1.0f;
	// 	if( mTextureIDs.find( mVA[i] ) != mTextureIDs.end() ) {
	// 		glBindTexture( GL_TEXTURE_2D, mTextureIDs[mVA[i]] );
	// 	}
	// 	else {
	// 		glBindTexture( GL_TEXTURE_2D, 0 );
	// 		useTexture = 0.0f;
	// 	}

	// 	glUniform1f( glGetUniformLocation( mGLProgram, "useTexture" ), useTexture );

	// 	glBindVertexArray( mVA[i] );
	// 	glDrawElements( GL_TRIANGLES, mNumIndices[i], GL_UNSIGNED_INT, 0 );
	// }

	// glBindVertexArray( 0 );

	glBindTexture( GL_TEXTURE_2D, mTargetTextures[0] );
	glBindVertexArray( mVA[0] );
	glBindFramebuffer( GL_FRAMEBUFFER, mFramebuffer );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTargetTextures[1], 0 );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glBindVertexArray( 0 );
}


/**
 * Handle resizing of the QWidget by updating the viewport and perspective.
 * @param {int} width  New width of the QWidget.
 * @param {int} height New height of the QWidget.
 */
void GLWidget::resizeGL( int width, int height ) {
	glViewport( 0, 0, width, height );
	mProjectionMatrix = glm::perspective(
		Cfg::get().value<float>( Cfg::PERS_FOV ),
		width / (float) height,
		Cfg::get().value<float>( Cfg::PERS_ZNEAR ),
		Cfg::get().value<float>( Cfg::PERS_ZFAR )
	);
	this->calculateMatrices();
}


/**
 * Calculate the current framerate and show it in the status bar.
 */
void GLWidget::showFPS() {
	mFrameCount++;

	GLuint currentTime = glutGet( GLUT_ELAPSED_TIME );
	GLuint timeInterval = currentTime - mPreviousTime;

	if( timeInterval > 1000 ) {
		float fps = mFrameCount / (float) timeInterval * 1000.0f;
		mPreviousTime = currentTime;
		mFrameCount = 0;

		char statusText[40];
		snprintf( statusText, 40, "%.2f FPS (%d\u00D7%dpx)", fps, width(), height() );
		( (Window*) parentWidget() )->updateStatus( statusText );
	}
}


/**
 * Set size of the QWidget.
 * @return {QSize} Width and height of the QWidget.
 */
QSize GLWidget::sizeHint() const {
	return QSize(
		Cfg::get().value<uint>( Cfg::WINDOW_WIDTH ),
		Cfg::get().value<uint>( Cfg::WINDOW_HEIGHT )
	);
}


/**
 * Select the next light in the list.
 */
void GLWidget::selectNextLight() {
	if( mSelectedLight > -1 ) {
		mSelectedLight = ( mSelectedLight + 1 ) % mLights.size();
	}
}


/**
 * Select the previous light in the list.
 */
void GLWidget::selectPreviousLight() {
	if( mSelectedLight > -1 ) {
		mSelectedLight = ( mSelectedLight == 0 ) ? mLights.size() - 1 : mSelectedLight - 1;
	}
}


/**
 * Start or resume rendering.
 */
void GLWidget::startRendering() {
	if( !mDoRendering ) {
		mDoRendering = true;
		float fps = Cfg::get().value<float>( Cfg::RENDER_INTERVAL );
		mTimer->start( fps );
	}
}


/**
 * Stop rendering.
 */
void GLWidget::stopRendering() {
	if( mDoRendering ) {
		mDoRendering = false;
		mTimer->stop();
		( (Window*) parentWidget() )->updateStatus( "Stopped." );
	}
}


/**
 * Switch between controlling the camera and the lights.
 */
void GLWidget::toggleLightControl() {
	if( mSelectedLight == -1 ) {
		if( mLights.size() > 0 ) {
			mSelectedLight = 0;
		}
	}
	else {
		mSelectedLight = -1;
	}
}
