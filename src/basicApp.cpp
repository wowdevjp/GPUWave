#include "cinder/app/AppBasic.h"
#include "cinder/Rand.h"
#include "cinder/MayaCamUI.h"
#include "cinder/Matrix.h"
#include "cinder/params/Params.h"
#include "cinder/gl/Vbo.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/qtime/MovieWriter.h"

using namespace ci;
using namespace ci::app;

#include <list>
using namespace std;

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>
#include <boost/lexical_cast.hpp>

#include <OpenCL/OpenCL.h>

#include "wave.cl.h"

namespace {
    static float remap(float value, float inputMin, float inputMax, float outputMin, float outputMax)
    {
        return (value - inputMin) * ((outputMax - outputMin) / (inputMax - inputMin)) + outputMin;
    }
    
    typedef struct
    {
        Vec4f position;
    }Vertex;
    
    const int WIDTH = 1000;
    const int DEPTH = 1000;
}

class BasicApp : public AppBasic {
 public:
    void prepareSettings( Settings *settings );
    void setup();
	void mouseDrag( MouseEvent event );
    void mouseDown(MouseEvent event);
	void keyDown( KeyEvent event );
    
    void resize( ResizeEvent event );
	void draw();
private:
    MayaCamUI m_camera;
    double m_lastTime;

    //パネル
    params::InterfaceGl m_interface;
    struct{
        float u_color_offset;
        float u_color_repeat_factor;
    }m_settings;
    
    dispatch_queue_t m_clqueue;
    dispatch_group_t m_group;
    
    gl::Vbo m_vbo;
    void *m_vbo_gpu;
    void *m_velocity_gpu;
    
    float m_time;
    
    float m_fps;
    
    gl::GlslProg m_shader;
    
    bool m_pause;
    
//    qtime::MovieWriter	mMovieWriter;
};


void BasicApp::prepareSettings( Settings *settings ) {
    settings->setWindowSize( 1024, 768 );
    settings->setFrameRate( 60.0f );
    
}
void BasicApp::setup()
{
    CameraPersp cam;
	cam.setEyePoint( Vec3f(40.0f, 10.0f, 40.0f) );
	cam.setCenterOfInterestPoint( Vec3f(0.0f, 2.5f, 0.0f) );
	cam.setPerspective( 60.0f, getWindowAspectRatio(), 1.0f, 1000.0f );
	m_camera.setCurrentCam( cam );
    
    Vertex *tmp = new Vertex[WIDTH * DEPTH];
    for(int z = 0 ; z < DEPTH ; ++z)
    {
        for(int x = 0 ; x < WIDTH ; ++x)
        {
            Vertex &vertex = tmp[z * DEPTH + x];
            vertex.position = Vec4f(remap(x, 0, WIDTH - 1, -50.0, 50.0),
                                    0.0f,
                                    remap(z, 0, DEPTH - 1, -50.0, 50.0),
                                    1.0f);
        }
    }
    m_vbo = gl::Vbo(GL_ARRAY_BUFFER);
    m_vbo.bufferData(sizeof(Vertex) * WIDTH * DEPTH, tmp, GL_STATIC_DRAW);
    delete[] tmp;
    
    //context bridge
    CGLContextObj glcontext = CGLGetCurrentContext();
    CGLShareGroupObj sharedgroup = CGLGetShareGroup(glcontext);
    gcl_gl_set_sharegroup(sharedgroup);
    m_clqueue = gcl_create_dispatch_queue(CL_DEVICE_TYPE_GPU, NULL);
    m_group = dispatch_group_create();
    
    m_vbo_gpu = gcl_gl_create_ptr_from_buffer(m_vbo.getId());
    
    dispatch_sync(m_clqueue, ^{
        m_velocity_gpu = gcl_malloc(sizeof(float) * WIDTH * DEPTH, NULL, 0);
        float *mapped = (float *)gcl_map_ptr(m_velocity_gpu, CL_MAP_WRITE, 0);
        for(int i = 0 ; i < WIDTH * DEPTH ; ++i)
        {
            mapped[i] = 0.0;
        }
        gcl_unmap(mapped);
    });
    
    m_time = 0.0f;
    
    __typeof__(m_settings) s = {0};
    s.u_color_offset = 0.5f;
    s.u_color_repeat_factor = 1.0f;
    m_settings = s;
    m_interface = params::InterfaceGl("info", Vec2i(200, 500));
    m_interface.addParam("fps", &m_fps);
    m_interface.addParam("u_color_offset", &m_settings.u_color_offset, "step=0.01");
    m_interface.addParam("u_color_repeat_factor", &m_settings.u_color_repeat_factor, "step=0.01");
    
    try
    {
        m_shader = gl::GlslProg(loadResource("vert.glsl"), loadResource("frag.glsl"));
    }
    catch(std::exception &e)
    {
        std::cout << e.what();
    }
    
    m_pause = false;
    
//    fs::path path("");
//    qtime::MovieWriter::Format format;
//	mMovieWriter = qtime::MovieWriter( path, getWindowWidth(), getWindowHeight(), format );
}
void BasicApp::resize( ResizeEvent event )
{
    CameraPersp cam = m_camera.getCamera();
	cam.setAspectRatio( getWindowAspectRatio() );
	m_camera.setCurrentCam( cam );
}
void BasicApp::mouseDrag( MouseEvent event )
{
    m_camera.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
}
void BasicApp::mouseDown(MouseEvent event)
{
    m_camera.mouseDown( event.getPos() );
}

void BasicApp::keyDown( KeyEvent event )
{
    if(event.getCode() == KeyEvent::KEY_SPACE)
    {
        m_pause = !m_pause;
    }
}

void BasicApp::draw()
{
    gl::enableDepthRead();
	gl::enableDepthWrite();
    
	gl::setMatrices( m_camera.getCamera() );
    
	gl::clear( Color( 0.1f, 0.1f, 0.1f ) );
    
    
    gl::color( Colorf(0.6f, 0.6f, 0.6f) );
    const float size = 50.0f;
    const float step = 5.0f;
	for(float i=-size;i<=size;i+=step) {
		gl::drawLine( Vec3f(i, 0.0f, -size), Vec3f(i, 0.0f, size) );
		gl::drawLine( Vec3f(-size, 0.0f, i), Vec3f(size, 0.0f, i) );
	}
    
    const float LEN = 50.0f;
    glColor3f( 1.0f, 0.0f, 0.0f );
    gl::drawLine(Vec3f::zero(), Vec3f(LEN, 0.0f, 0.0f));
    glColor3f( 0.0f, 1.0f, 0.0f );
    gl::drawLine(Vec3f::zero(), Vec3f(0.0f, LEN, 0.0f));
    glColor3f( 0.0f, 0.0f, 1.0f );
    gl::drawLine(Vec3f::zero(), Vec3f(0.0f, 0.0f, LEN));
    

    dispatch_group_wait(m_group, DISPATCH_TIME_FOREVER);
    
    m_vbo.bind();
    m_shader.bind();
    m_shader.uniform("u_color_offset", m_settings.u_color_offset);
    m_shader.uniform("u_color_repeat_factor", m_settings.u_color_repeat_factor);
    
    glColor3f(cinder::Color::white());
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(4, GL_FLOAT, sizeof(Vertex), 0);
    glDrawArrays(GL_POINTS, 0, WIDTH * DEPTH);
    glDisableClientState(GL_VERTEX_ARRAY);
    
    m_shader.unbind();
    m_vbo.unbind();
        
    glFinish();
    
    float time = m_time;
    float x = sin(0.2 * time * 2.5f) * 40.0f;
    float z = sin(0.2 * time * 3.5f) * 40.0f;
    
    glColor3f(1.0f, 0.0f, 0.0f);
    gl::drawSphere(Vec3f(x, 0.0f, z), 2.0f);
    
    if(m_pause)
    {
        
    }
    else  
    {
        m_time += 0.05f;
        
        dispatch_group_async(m_group, m_clqueue, ^{
            cl_ndrange ndrange = {
                2, /*number of dimensions*/
                {0, 0, 0}, /*offset*/
                {WIDTH, DEPTH, 0}, /*range each demensions */
                {0, 0, 0}, /*?*/
            };
            
            calc_wave_kernel(&ndrange, (cl_float4 *)m_vbo_gpu, (cl_float *)m_velocity_gpu, WIDTH, DEPTH);
            
            effect_wave_kernel(&ndrange, (cl_float4 *)m_vbo_gpu, (cl_float *)m_velocity_gpu, x, z);
        });
    }
    

    
    m_fps = getAverageFps();
    
    params::InterfaceGl::draw();
    
//    mMovieWriter.addFrame(copyWindowSurface());
}

// This line tells Flint to actually create the application
CINDER_APP_BASIC( BasicApp, RendererGl )