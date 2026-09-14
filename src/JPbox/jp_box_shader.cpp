#include "jp_box_shader.h"
#include "../JPutils/jp_shader_globals.h"
#include <iostream>
#include <sstream>
#include <filesystem>


namespace {
bool linkedProgram(ofShader& shader) {
    if (!shader.isLoaded() || !shader.getProgram()) return false;
    GLint linked = GL_FALSE;
    glGetProgramiv(shader.getProgram(), GL_LINK_STATUS, &linked);
    return linked == GL_TRUE;
}
}

JPbox_shader::JPbox_shader()
{
	advancedMappingMaskDirty.fill(true);
}
JPbox_shader::~JPbox_shader()
{
	clearAdvancedMappingResources();
}

void JPbox_shader::reload()
{
	// cout << "RELOD SHADER " << endl;

	JPParameterGroup auxparameters;
	auxparameters = parameters;
	// parameters.clear();

	JPFbohandlerGroup auxfbohandler;
	auxfbohandler = fbohandlergroup;

	// parameters.clear();
	// fbohandlergroup.clear();
	//	cout << "auxfbohandler ANTES" << endl;
	// cout << "-----------------------------" << endl;
	/*for (int i = 0; i < auxfbohandler.getSize(); i++) {
		cout << "NAME :" << auxfbohandler.getFboName(i) << endl;
	}*/

	cout << "param size A" << parameters.getSize() << endl;

	// One bounded attempt. Editors can briefly leave a source empty while
	// saving; retrying until it has uniforms used to block the render thread.
    const ofBuffer source = ofBufferFromFile(dir);
    ofShaderSettings candidateSettings;
    candidateSettings.shaderFiles[GL_VERTEX_SHADER] = ofToDataPath("shaders/default.vert", true);
    candidateSettings.shaderSources[GL_FRAGMENT_SHADER] = source.getText();
    candidateSettings.sourceDirectoryPath = ofFilePath::getEnclosingDirectory(ofToDataPath(dir, true));
    ofShader candidateShader;
    if (source.size() == 0 || !candidateShader.setup(candidateSettings) || !linkedProgram(candidateShader)) {
        uniformDiagnostics = {{jp_uniform_parser::Severity::Error,
            jp_uniform_parser::Code::ShaderCompileError, {1, 1}, "",
            "Shader compilation failed; current program and controls kept. See compiler log."}};
        ofLogError("shader-reload") << "Compilation failed; current shader and controls kept: " << dir;
        return;
    }
	if (!setUniforms(parameters, fbohandlergroup, dir, name, &source)) return;

	/*cout << "----------------------------------"; endl;
	cout << "Variables anteriores" << endl;
	for (int i = 0; i < auxparameters.getSize(); i++) {
		string nombre = auxparameters.getName(i);
		string valor = ofToString(auxparameters.getFloatValue(i));
		cout << "nombre : " << nombre << "valor" << valor << endl;
	}
	for (int i = 0; i < auxfbohandler.getSize(); i++) {
		cout << "NAME :" << auxfbohandler.getFboName(i) << endl;
	}

	cout << "Variables despues " << endl;
	for (int i = 0; i < parameters.getSize(); i++) {
		string nombre = parameters.getName(i);
		string valor = ofToString(parameters.getFloatValue(i));
		cout << "nombre : " << nombre << "valor" << valor << endl;
	}

	for (int i = 0; i < auxfbohandler.getSize(); i++) {
		cout << "NAME :" << auxfbohandler.getFboName(i) << endl;
	}*/
	// Esto es para que si agrego un uniform no me randomice los valores que ya estaban seteados.
	// int index = 0;
	// cout << "auxfbohandler DESPUES" << endl;
	// cout << "-----------------------------" << endl;
	/*for (int i = 0; i < auxfbohandler.getSize(); i++) {
		//cout << "NAME :" << auxfbohandler.getFboName(i) <<endl;
	}*/

	// cout << "fbohandlergroup" << endl;
	// cout << "-----------------------------" << endl;
	/*for (int i = 0; i < fbohandlergroup.getSize(); i++) {
		//cout << "NAME :" << fbohandlergroup.getFboName(i) <<endl;
	}*/
	for (int i = 0; i < auxparameters.getSize(); i++)
	{
		for (int k = 0; k < parameters.getSize(); k++)
		{
			if (ofTrim(parameters.getName(k)) == ofTrim(auxparameters.getName(i)) &&
				parameters.getType(k) == auxparameters.getType(i))
			{
				if (parameters.getType(k) == parameters.BOOL)
				{
					parameters.setBoolValue(auxparameters.getBoolValue(i), k);
				}
				else if (parameters.getType(k) == parameters.FLOAT)
				{
					parameters.setFloatValue(auxparameters.getFloatValue(i), k);
					parameters.setFloatLerpValue(auxparameters.getLerpValue(i), k);
					parameters.setRangeMin(auxparameters.getRangeMin(i), k);
					parameters.setRangeMax(auxparameters.getRangeMax(i), k);
					parameters.setRangeEnabled(
						auxparameters.getJParameter(i)->rangeEnabled, k);
					parameters.setSpeed(auxparameters.getSpeed(i), k);
					parameters.setBpmRate(auxparameters.getBpmRate(i), k);
					const int previousMoveType =
						auxparameters.getMovType(i);
					const bool canRestoreMoveType =
						previousMoveType != JPParameter::BPM ||
						parameters.getJParameter(k)->bpmEligible;
					parameters.setmovetype(
						canRestoreMoveType ? previousMoveType :
							JPParameter::STANDART,
						k);
					parameters.setlastmovetype(
						auxparameters.getLastMovType(i), k);
				}
				JPParameter *destination = parameters.getJParameter(k);
				JPParameter *source = auxparameters.getJParameter(i);
				destination->audioSource = source->audioSource;
				destination->audioDiv = source->audioDiv;
				destination->audioBase = source->audioBase;
				destination->audioAmount = source->audioAmount;
				destination->audioInvert = source->audioInvert;
				destination->audioThreshold = source->audioThreshold;
				destination->audioCurve = source->audioCurve;
				destination->audioAttackMs = source->audioAttackMs;
				destination->audioReleaseMs = source->audioReleaseMs;
				destination->audioShapingOpen = source->audioShapingOpen;
				destination->audioDrivesSpeed = source->audioDrivesSpeed;
				destination->audioSpeedDirection = source->audioSpeedDirection;
				destination->randomLocked = source->randomLocked;
				destination->defaultFloatValue = source->defaultFloatValue;
				destination->defaultBoolValue = source->defaultBoolValue;
			}
		}
	}
	/*for (int k = 0; k < parameters.getSize(); k++) {
		string nombre = parameters.getName(k);
		string valor = ofToString(parameters.getFloatValue(k));
		cout << "nombre : " << nombre << "valor" << valor << endl;
		if (parameters.getType(k) == parameters.FLOAT) {
			parameters.setFloatValue(0.0, k);
		}
	}*/
	// Esto es para que no rompa las conexiones si reinicio el shader
	// int index = 0;
	for (int i = 0; i < auxfbohandler.getSize(); i++)
	{
		for (int k = 0; k < fbohandlergroup.getSize(); k++)
		{
			if (ofTrim(fbohandlergroup.getName(k)) == ofTrim(auxfbohandler.getName(i)) &&
				auxfbohandler.getisPointerSet(i))
			{
				fbohandlergroup.setFboPointer(auxfbohandler.getFboPointerReference(i),
											  auxfbohandler.getFboNameReference(i), k);
			}
			else
			{
			}
		}
	}
	shader = candidateShader;
	resetFeedbackFrame();

	fbohandlergroup.setupdragobjects(x, y, outlet_size, outlet_size);
	setfbohandler_nodepos();
	frameNum = 0;
	
}
void JPbox_shader::reloadShaderonly()
{
    ofShader candidate;
    if (!candidate.load("shaders/default.vert", dir) || !linkedProgram(candidate)) return;
    shader = candidate;
	resetFeedbackFrame();
	frameNum = 0;
}
void JPbox_shader::setup(ofTrueTypeFont &_font,
						 string _dir,
						 string _nombre)
{
	JPbox::setup(_font);
	setUniforms(parameters, fbohandlergroup, _dir, _nombre);
	// parameters.coutData();
	name = _nombre;
	dir = _dir;
	if (isAdvancedMappingShader())
	{
		initializeAdvancedMappingState();
	}
	showCode = true;
	try
	{
		shader.load("shaders/default.vert", dir);
	}
	catch (int e)
	{
		cout << "ERROR EL SHADER NO LEVANTO " << endl;
	}
	if (shader.isLoaded())
	{
		cout << "CARGO BIEN EL SHADER " << endl;
	}
	else
	{
		cout << "FALLOO HEAVY " << endl;
	}

	tipo = SHADERBOX;
	fbohandlergroup.setupdragobjects(x, y, outlet_size, outlet_size);
	setfbohandler_nodepos();

	/*if (!ofFile(dir).exists())
	{
		std::cerr << dir << " does not exist!" << std::endl;
		return;
	}*/

	datemodified = std::filesystem::last_write_time(ofToDataPath(dir));
}

void JPbox_shader::setup2(string _dir,
						  string _nombre)
{
	cout << "CORRE SETUP DE SHADER " << endl;

	JPbox::setup(_dir, _nombre);
	setUniforms(parameters, fbohandlergroup, _dir, _nombre);
	// parameters.coutData();
	name = _nombre;
	dir = _dir;
	if (isAdvancedMappingShader())
	{
		initializeAdvancedMappingState();
	}
	try
	{
		shader.load("shaders/default.vert", dir);
	}
	catch (int e)
	{
		cout << "ERROR EL SHADER NO LEVANTO " << endl;
	}
	if (shader.isLoaded())
	{
		cout << "CARGO BIEN EL SHADER " << endl;
	}
	else
	{
		cout << "FALLOO HEAVY " << endl;
	}

	tipo = SHADERBOX;
	fbohandlergroup.setupdragobjects(x, y, outlet_size, outlet_size);
	setfbohandler_nodepos();

	if (!ofFile(dir).exists())
	{
		std::cerr << dir << " does not exist!" << std::endl;
		return;
	}

	datemodified = std::filesystem::last_write_time(ofToDataPath(dir));
}

void JPbox_shader::setup(string _dir,
						 string _nombre)
{
	cout << "CORRE SETUP DE SHADER " << endl;
	JPbox::setup(_dir, _nombre);
	setUniforms(parameters, fbohandlergroup, _dir, _nombre);
	// parameters.coutData();
	name = _nombre;
	dir = _dir;
	if (isAdvancedMappingShader())
	{
		initializeAdvancedMappingState();
	}
	try
	{
		shader.load("shaders/default.vert", dir);
	}
	catch (int e)
	{
		cout << "ERROR EL SHADER NO LEVANTO " << endl;
	}
	if (shader.isLoaded())
	{
		cout << "CARGO BIEN EL SHADER " << endl;
	}
	else
	{
		cout << "FALLOO HEAVY " << endl;
	}

	tipo = SHADERBOX;
	fbohandlergroup.setupdragobjects(x, y, outlet_size, outlet_size);
	setfbohandler_nodepos();

	if (!ofFile(_dir).exists())
	{
		std::cerr << _dir << " does not exist!" << std::endl;
		return;
	}

	datemodified = std::filesystem::last_write_time(ofToDataPath(dir));
}

void JPbox_shader::draw()
{
	ofSetRectMode(OF_RECTMODE_CORNER);
	JPbox::draw();
	fbo.draw(x, y + padding_top / 2 - 3, fbowidth, fboheight);
	JPbox::draw_outlet();

	// DIBUJAR NODOS:
	draw_inlets();

	ofSetColor(255, 255);
}

void JPbox_shader::clear()
{

	JPbox::clear();
	// font_p = nullptr;
	parameters.clear();

	cout << "CORRE CLEAR SHADERBOX " << endl;
	fbo.clear();
	fbo.destroy();
	// fbohandlergroup.clear();
	shader.unload();
	clearAdvancedMappingResources();
}
void JPbox_shader::update()
{
	JPbox::update();
	if (shouldRenderThisFrame())
	{
		updateFBO();
	}
	frameNum++;
}
void JPbox_shader::updateFBO()
{
	// SHADER RENDER UPDATE
	if (tryPassThroughFBO())
	{
		return;
	}
	if (onoff.boolValue && shader.isLoaded())
	{
		prepareFeedbackFrame(shader);
		if (isAdvancedMappingShader())
		{
			for (int layerIndex = 0;
				 layerIndex < ADVANCED_MAPPING_LAYER_COUNT;
				 layerIndex++)
			{
				// Same name based test updateAdvancedMappingUniforms uses, so
				// the mask and the connected flag can never disagree.
				const int inletIndex =
					getAdvancedMappingInletIndex(layerIndex);
				if (inletIndex < 0 ||
					!fbohandlergroup.getisPointerSet(inletIndex))
					continue;
				const bool maskSizeChanged = advancedMappingMasks[layerIndex].isAllocated() &&
					(advancedMappingMasks[layerIndex].getWidth() != fbo.getWidth() ||
					 advancedMappingMasks[layerIndex].getHeight() != fbo.getHeight());
				if (advancedMappingMaskDirty[layerIndex] ||
					!advancedMappingMasks[layerIndex].isAllocated() || maskSizeChanged)
					rebuildAdvancedMappingMask(layerIndex);
			}
		}
		ofPushStyle();
		ofSetRectMode(OF_RECTMODE_CORNER);
		ofSetColor(255, 255);
		fbo.begin();
		// A fragment shader already produces the complete destination pixel.
		// Alpha-blending that result over the box's previous frame accumulates
		// transparent media and creates duplicated silhouettes/backgrounds.
		ofEnableBlendMode(OF_BLENDMODE_DISABLED);
		shader.begin();
		update_globalUniforms();
		update_NonglobalUniforms();
		if (isAdvancedMappingShader())
		{
			updateAdvancedMappingUniforms();
		}
		// White, not red: the bound shader ignores the global colour, but this
		// leaks out of the fbo as global GL state and tinted every drawer that
		// ran later in the frame without setting its own colour.
		ofSetColor(255, 255);
		ofRect(0, 0, fbo.getWidth(), fbo.getHeight());
		
		shader.end();
		fbo.end();
		ofEnableAlphaBlending();
		ofPopStyle();

		fbo.begin();
		//ofSetColor(255, 0, 0);
		//ofDrawEllipse(ofGetMouseX(), ofGetMouseY(), 50, 50);
		if (showCode) {
			// Consigue el texto del buffer
			std::string text = buffer.getText();
			float textHeight = jp_constants::h_font.stringHeight(text) ; // Asumiendo que el texto es multilinea
			//float visibleHeight = ofGetHeight() - 200; // Altura visible donde el texto se muestra

			//ofDrawBitmapString(text, 0, 0);
			jp_constants::h_font.drawString(text,
				50,
				sin(ofGetElapsedTimeMillis() * 0.00001) * textHeight/2 -textHeight/2 );
			
		}
		fbo.end();
	}
	else
	{
		JPbox::updateFBO();
	}
}
bool JPbox_shader::setUniforms(JPParameterGroup &_parameters,
	JPFbohandlerGroup &_fbohandlergroup, string _dir, string _name, const ofBuffer* source)
{
	using namespace jp_uniform_parser;
	const ofBuffer candidate = source ? *source : ofBufferFromFile(_dir);
	if (candidate.size() == 0)
	{
		uniformDiagnostics = {{Severity::Error, Code::SourceReadError, {1, 1}, "",
			"Shader source is empty or could not be read; existing controls were kept."}};
		ofLogError("uniform-parser") << _dir << ":1:1: " << uniformDiagnostics.front().message;
		return false;
	}
	const Result parsed = parse(candidate.getText());
	uniformDiagnostics = parsed.diagnostics;
	for (const Diagnostic &diagnostic : uniformDiagnostics)
	{
		// Unsupported globals are supplied by the renderer, not GUI controls.
		if ((diagnostic.code == Code::UnsupportedType || diagnostic.code == Code::UnsupportedArray) &&
			jp_shader_globals::isGlobalName(diagnostic.name)) continue;
		const string message = _dir + ":" + ofToString(diagnostic.location.line) +
			":" + ofToString(diagnostic.location.column) + ": " + diagnostic.message;
		if (diagnostic.severity == Severity::Error) ofLogError("uniform-parser") << message;
		else ofLogWarning("uniform-parser") << message;
	}
	if (parsed.hasErrors()) return false;

	_parameters.clear();
	_fbohandlergroup.clear();
	_parameters.setName(_name);
	buffer = candidate;
	for (const Declaration &declaration : parsed.declarations)
	{
		if (declaration.internal || declaration.array) continue;
		if (declaration.type == Type::Float)
		{
			// Preserve the legacy random draw even for explicit defaults and
			// excluded audio globals: subsequent defaults/seeds depend on it.
			const float fallback = ofRandom(1);
			if (jp_shader_globals::isNewGlobalName(declaration.name)) continue;
			_parameters.addFloatValue(declaration.floatDefault.value_or(fallback),
				declaration.name, true);
			JPParameter *added = _parameters.getJParameter(_parameters.getSize() - 1);
			if (jp_media::isScaleRatioParameter(declaration.name))
			{
				added->nativeMin = added->min = 0.1f;
				added->nativeMax = added->max = 4.0f;
				added->defaultFloatValue = 1.0f;
			}
			JPParameter::parseColorAnnotation(declaration.annotations,
				added->colorChannel, added->colorGroup);
		}
		else if (declaration.type == Type::Bool)
			_parameters.addBoolValue(declaration.boolDefault.value_or(false), declaration.name);
		else if (declaration.type == Type::Sampler2D || declaration.type == Type::Sampler2DRect)
			_fbohandlergroup.addFbohandler(declaration.name);
	}
	return true;
}
void JPbox_shader::setfbohandler_nodepos()
{
	for (int i = 0; i < fbohandlergroup.getSize(); i++)
	{
		float x_e = x - width / 2;
		float y_e = y;
		if (fbohandlergroup.getSize() > 1)
		{
			y_e = y + ofMap(i, 0, fbohandlergroup.getSize() - 1, -(height / 2) * 3 / 6, (height / 2) * 3 / 6);
		}
		fbohandlergroup.setPos(x_e, y_e, i);
	}
}
void JPbox_shader::update_NonglobalUniforms()
{
	for (int i = 0; i < parameters.getSize(); i++)
	{
		if (parameters.getType(i) == parameters.FLOAT)
		{
			shader.setUniform1f(parameters.getName(i), parameters.getFloatValue(i));
		}
		else if (parameters.getType(i) == parameters.BOOL)
		{
			shader.setUniform1f(parameters.getName(i), parameters.getBoolValue(i));
		}
	}
	for (int i = 0; i < fbohandlergroup.getSize(); i++)
	{
		if (fbohandlergroup.getisPointerSet(i))
		{
			shader.setUniformTexture(fbohandlergroup.getName(i), fbohandlergroup.getFboPointer(i), i + 1);
		}
	}
}
void JPbox_shader::update_globalUniforms()
{
	JPShaderGlobalsCtx ctx;
	ctx.width = fbo.getWidth();
	ctx.height = fbo.getHeight();
	ctx.boxFrameNum = frameNum;
	ctx.feedback = getFeedbackTexture();
	jp_shader_globals::apply(shader, ctx);
}

/*********************************DEPRECATED ******************************************/
/*JPParameterGroup JPbox_shader::getUniformsToJPParameterGroup(string _dir, string _name) {
	vector < string > linesOfTheFile;
	ofBuffer buffer = ofBufferFromFile(_dir);
	for (auto line : buffer.getLines()) {
		linesOfTheFile.push_back(line);
	}
	JPParameterGroup group;//TESTGROUP
	group.setName(_name);
	for (int l = 0; l < linesOfTheFile.size(); l++) {
		if (linesOfTheFile[l].rfind("uniform", 0) == 0) {
			if (linesOfTheFile[l].find("float") != std::string::npos) {
				//cout << "FLOAT NAME : " << name << endl;
				string name(linesOfTheFile[l], 14, linesOfTheFile[l].size()); // 6 letras de texto1, desde la tercera
																			  //name.substr(0, name.size() - 1);
				name.pop_back();//Le resta el ultimo caracter
				group.addFloatValue(ofRandom(1), name);
			}
			if (linesOfTheFile[l].find("sampler2D") != std::string::npos) {
				string name(linesOfTheFile[l], 18, linesOfTheFile[l].size());



			}
			if (linesOfTheFile[l].find("bool") != std::string::npos) {
				string name(linesOfTheFile[l], 13, linesOfTheFile[l].size());

				name = name.substr(0, name.find(";"));
				group.addBoolValue(false, name);
			}
		}
	}
	return group;
}*/
