#include <iostream>
#include <fstream>
#include <cmath>
#include <cassert>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>

bool pressed = false;

cv::Point3_<double> estimate_light_direction(cv::Mat sphere) {
  const int height = sphere.rows;
  const int width  = sphere.cols;

  const cv::Point center(width/2, height/2);

  int first_hit_y = 0, last_hit_y = 0;
  int brightest_x_sum = 0, brightest_y_sum = 0, points = 0;
  uint8_t brightest = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      uint8_t val = sphere.at<uint8_t>(cv::Point(x, y));
      if (val > 0 && first_hit_y)
        first_hit_y = y;
      if (val > 0)
        last_hit_y = y;
      if (val > brightest) {
        points = 1;
        brightest_x_sum = x;
        brightest_y_sum = y;
        brightest = val;
      } else if (val == brightest) {
        points++;
        brightest_x_sum += x;
        brightest_y_sum += y;
      }
    }
  }

  const double size = (last_hit_y - first_hit_y) / 2.0;

  const cv::Point direction_2d = cv::Point((double)brightest_x_sum/(double)points, (double)brightest_y_sum/(double)points) - center;
  double x = (double)direction_2d.x / size;
  double y = (double)direction_2d.y / size;
  double z = sqrt(size*size - (double)direction_2d.dot(direction_2d)) / size;

  assert(x*x + y*y + z*z - 1.0 < 0.1e-8);

  return cv::Point3_<double>(x, y, z);
}

cv::Point3_<double> calc_normal(cv::Matx34d s, cv::Vec<double, 4> i) {
  cv::Mat ms(s);
  cv::Mat normalize = ms * ms.t();
  cv::Mat vec = ms * cv::Mat(i);
  cv::Mat n = (normalize).inv() * vec;
  cv::Vec<double, 3> nvec(n.at<double>(0, 0),
                           n.at<double>(1, 0),
                           n.at<double>(2, 0));
  double scale = 1.0/sqrt(nvec.dot(nvec));
  return cv::Point3_<double>(nvec.mul(cv::Vec<double, 3>(scale, scale, scale)));
}

struct result_set {
  int width;
  int height;
  cv::Point3_<double> *normal;
  double *height_map;
};

struct result_set *result = NULL;

void init(void)
{
  glClearColor(1.0, 1.0, 1.0, 1.0);
}

void display(void)
{
  glClear(GL_COLOR_BUFFER_BIT);

  if (result == NULL)
    goto end;

  glPushMatrix();
  glTranslated( - result->width/2, + result->height/2, 0);
  glColor3d(0.0, 0.0, 0.0);
  glBegin(GL_LINES);
  for (int y = 0; y < result->height; ++y) {
    for (int x = 0; x < result->width; ++x) {
      GLdouble gl_vec[3] = {(double)x, - (double)y, result->height_map[y*result->width + x]};

      glVertex3dv(gl_vec);
      if (0 < x && x < result->width - 1)
        glVertex3dv(gl_vec);
    }
  }
  for (int x = 0; x < result->width; ++x) {
    for (int y = 0; y < result->height; ++y) {
      GLdouble gl_vec[3] = {(double)x, - (double)y, result->height_map[y*result->width + x]};

      glVertex3dv(gl_vec);
      if (0 < y && y < result->height - 1)
        glVertex3dv(gl_vec);
    }
  }
  glEnd();
  glPopMatrix();

 end:
  glFlush();
}

struct mouse_status {
  int x, y;
  bool left, right;
};

mouse_status ms;

void mouse(int button, int state, int x, int y)
{
  int d_z = 0;

  ms.x = x;
  ms.y = y;
  switch (button) {
  case 0:                     // left
    ms.left = state == 0;
    break;
  case 2:                     // right
    ms.right = state == 0;
    break;
  case 3:                     // wheel up
    if (state == 0)
      d_z = 5;
    break;
  case 4:                     // wheel down
    if (state == 0)
      d_z = - 5;
    break;
  }
  glTranslated(0, 0, d_z);
  glutPostRedisplay();
}

void motion(int x, int y)
{
  if (ms.left) {
    glTranslated((x - ms.x)/10.0, - (y - ms.y)/10.0, 0);
    glutPostRedisplay();
  }
  if (ms.right) {
    glRotated((x - ms.x)/2.0, 0, 0, 1.0);
    glRotated((y - ms.y)/2.0, 1, 0, 0);
    glutPostRedisplay();
  }
  ms.x = x;
  ms.y = y;
}

void resize(int w, int h)
{
  glViewport(0, 0, w, h);

  glLoadIdentity();
  gluPerspective(30.0, (double)w / (double)h, 1.0, 1000.0);
  glTranslated(0, 0, - 200);
  glRotated(-50, 1.0, 0, 0.0);
}

int main(int argc, char *argv[])
{
  cv::Matx34d directions;

  for (int i = 0; i < 4; ++i) {
    char name[256];
    sprintf(name, "./dist/Ball%d.bmp", i + 1);
    cv::Mat_<uint8_t> sphere = cv::imread(name, CV_LOAD_IMAGE_GRAYSCALE);
    cv::Vec<double, 3> d = estimate_light_direction(sphere);
    for (int j = 0; j < 3; ++j) {
      directions(j, i) = d(j);
    }
  }

  cv::Mat problem[4];
  for (int i = 0; i < 4; ++i) {
    char name[256];
    sprintf(name, "./dist/Object%d.bmp", i + 1);
    problem[i] = cv::imread(name, CV_LOAD_IMAGE_GRAYSCALE);
  }
  const int width = problem[0].cols;
  const int height = problem[0].rows;

  cv::Point3_<double> *normal = new cv::Point3_<double>[width*height];

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      cv::Vec<double, 4> i(
        (double)problem[0].at<uint8_t>(y, x),
        (double)problem[1].at<uint8_t>(y, x),
        (double)problem[2].at<uint8_t>(y, x),
        (double)problem[3].at<uint8_t>(y, x));
      normal[y*width + x] = calc_normal(directions, i);
    }
  }

  double *height_map = new double[width*height];
  double smallest = 0;
  cv::Point3_<double> n;
  std::ofstream ofs("result.txt");
  height_map[0] = 0;

  for (int y = 1; y < height; ++y) {
    n = normal[y*width] + normal[(y-1)*width];
    height_map[y*width] = height_map[(y-1)*width] - (n.y/n.z);
    if (height_map[y*width] < smallest)
      smallest = height_map[y*width];
  }

  for (int x = 1; x < width; ++x) {
    n = normal[x] + normal[x-1];
    height_map[x] = height_map[x-1] - (n.x/n.z);
    if (height_map[x] < smallest)
      smallest = height_map[x];
  }

  for (int y = 1; y < height; ++y) {
    for (int x = 1; x < width; ++x) {
      cv::Point3_<double> nx;
      cv::Point3_<double> ny;
      double zx, zy;
      nx = normal[y*width + x] + normal[y*width + x-1];
      ny = normal[y*width + x] + normal[(y-1)*width + x];
      zx = height_map[y*width + (x-1)] - (nx.x/nx.z);
      zy = height_map[(y-1)*width + x] - (ny.y/ny.z);
      height_map[y*width + x] = (zx + zy)/2.0;
      if (height_map[y*width + x] < smallest)
        smallest = height_map[y*width + x];
    }
  }

  for (int i = 0; i < height*width; ++i) {
    height_map[i] = height_map[i] - smallest;
    ofs << height_map[i] << std::endl;
  }

  result = new result_set;
  result->width  = width;
  result->height = height;
  result->normal = normal;
  result->height_map = height_map;

  if (argc > 1) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA);
    glutCreateWindow("glut");
    glutDisplayFunc(display);
    glutReshapeFunc(resize);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    init();
    glutMainLoop();
  }

  delete normal;
  delete height_map;
  delete result;
  return 0;
}
