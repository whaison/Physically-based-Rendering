#include "Cfg.h"


const char* Cfg::CAM_SPEED = "camera.speed";
const char* Cfg::LOGGING = "logging";
const char* Cfg::RENDER_INTERVAL = "render.interval";
const char* Cfg::PERS_FOV = "render.perspective.fov";
const char* Cfg::PERS_ZFAR = "render.perspective.zfar";
const char* Cfg::PERS_ZNEAR = "render.perspective.znear";
const char* Cfg::SHADER_PATH = "shader.path";
const char* Cfg::SHADER_NAME = "shader.name";
const char* Cfg::WINDOW_HEIGHT = "window.height";
const char* Cfg::WINDOW_WIDTH = "window.width";


/**
 * Load the config file (JSON).
 * @param {const char*} filepath File path.
 */
void Cfg::loadConfigFile( const char* filepath ) {
	boost::property_tree::json_parser::read_json( filepath, mPropTree );
}