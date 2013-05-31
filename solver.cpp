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
  std::cout << (brightest_x_sum/points) << ", " << (brightest_y_sum/points) << std::endl;

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
  glBegin(GL_POINTS);
  for (int y = 0; y < result->height; ++y) {
    for (int x = 0; x < result->width; ++x) {
      GLdouble gl_vec[3] = {(double)x, - (double)y, result->height_map[y*result->width + x]};

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
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_RGBA);
  glutCreateWindow("glut");
  glutDisplayFunc(display);
  glutReshapeFunc(resize);
  glutMouseFunc(mouse);
  glutMotionFunc(motion);
  init();

  std::cout << "For Computer Vision class Assignment" << std::endl;

  cv::Mat_<uint8_t> spheres = cv::imread("./sphere.png", CV_LOAD_IMAGE_GRAYSCALE);

  cv::Matx34d directions;

  for (int i = 0; i < 4; ++i) {
    cv::Vec<double, 3> d = estimate_light_direction(spheres(cv::Range(0, 320), cv::Range(240*i, 240*(i+1))));
    std::cout << cv::Mat(d) << std::endl;
    for (int j = 0; j < 3; ++j) {
      directions(j, i) = d(j);
    }
  }
  std::cout << cv::Mat(directions) << std::endl;

  cv::Mat_<uint8_t> problem = cv::imread("./problem.png", CV_LOAD_IMAGE_GRAYSCALE);
  const int width = problem.cols / 4;
  const int height = problem.rows;

  cv::Point3_<double> *normal = new cv::Point3_<double>[width*height];

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      cv::Vec<double, 4> i(
        (double)problem.at<uint8_t>(y, x),
        (double)problem.at<uint8_t>(y, x + width),
        (double)problem.at<uint8_t>(y, x + width*2),
        (double)problem.at<uint8_t>(y, x + width*3));
      normal[y*width + x] = calc_normal(directions, i);
    }
  }

  double *height_map = new double[width*height];
  double value;
  cv::Point3_<double> n;
  std::ofstream f_y0("y0.txt");
  std::ofstream f_ylast("ylast.txt");
  height_map[0] = 0;

  for (int y = 1; y < height; ++y) {
    n = normal[y*width] + normal[(y-1)*width];
    value = height_map[y*width] = height_map[(y-1)*width] - (n.y/n.z);
    f_y0 << y << ", " << value << ", " << n << std::endl;
  }

  for (int x = 1; x < width; ++x) {
    n = normal[x] + normal[x-1];
    height_map[x] = height_map[x-1] - (n.x/n.z);
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
      value = height_map[y*width + x] = (zx + zy)/2.0;
      if (x == width-1)
        f_ylast << y << ", " << value << ", " << n << std::endl;
    }
  }

  result = new result_set;
  result->width  = width;
  result->height = height;
  result->normal = normal;
  result->height_map = height_map;

  glutMainLoop();
  cv::waitKey(0);

  delete normal;
  delete height_map;
  delete result;
  return 0;
}
