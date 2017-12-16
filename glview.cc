/*
 *  OpenSCAD (www.openscad.at)
 *  Copyright (C) 2009  Clifford Wolf <clifford@clifford.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "openscad.h"

#include <QWheelEvent>
#include <QMouseEvent>

#define FAR_FAR_AWAY 100000.0

GLView::GLView(QWidget *parent) : QGLWidget(parent) {
  viewer_distance = 500;
  object_rot_y = 35;
  object_rot_z = 25;

  mouse_drag_active = false;
  last_mouse_x = 0;
  last_mouse_y = 0;

  renderfunc = NULL;
  renderfunc_vp = NULL;

  for (int i = 0; i < 10; i++)
    shaderinfo[i] = 0;

  setMouseTracking(true);
}

extern GLint e1, e2, e3;

void GLView::initializeGL() {
  glEnable(GL_DEPTH_TEST);
  glDepthRange(-FAR_FAR_AWAY, +FAR_FAR_AWAY);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glClearColor(1.0, 1.0, 0.9, 0.0);

}

void GLView::resizeGL(int w, int h) {
  glViewport(0, 0, w, h);
  w_h_ratio = sqrt((double) w / (double) h);
}

void GLView::paintGL() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(-w_h_ratio, +w_h_ratio, -(1 / w_h_ratio), +(1 / w_h_ratio), +10.0, +FAR_FAR_AWAY);
  gluLookAt(0.0, -viewer_distance, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  GLfloat light_diffuse[] = {1.0, 1.0, 1.0, 1.0};
  GLfloat light_position0[] = {-1.0, -1.0, +1.0, 0.0};
  GLfloat light_position1[] = {+1.0, +1.0, -1.0, 0.0};

  glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, light_position0);
  glEnable(GL_LIGHT0);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, light_diffuse);
  glLightfv(GL_LIGHT1, GL_POSITION, light_position1);
  glEnable(GL_LIGHT1);
  glEnable(GL_LIGHTING);
  glEnable(GL_NORMALIZE);

  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glEnable(GL_COLOR_MATERIAL);

  glRotated(object_rot_y, 1.0, 0.0, 0.0);
  glRotated(object_rot_z, 0.0, 0.0, 1.0);

  glDepthFunc(GL_LESS);

#if 0
  glLineWidth(1);
  glColor3d(0.0, 0.0, 1.0);
  glBegin(GL_LINES);
  glVertex3d(0, 0, 0);
  glVertex3d(10, 0, 0);
  glVertex3d(0, 0, 0);
  glVertex3d(0, 10, 0);
  glVertex3d(0, 0, 0);
  glVertex3d(0, 0, 10);
  glEnd();
#endif

  glLineWidth(2);
  glColor3d(1.0, 0.0, 0.0);

  if (renderfunc)
    renderfunc(renderfunc_vp);
}

void GLView::wheelEvent(QWheelEvent *event) {
  viewer_distance *= pow(0.9, event->delta() / 120.0);
  updateGL();
}

void GLView::mousePressEvent(QMouseEvent *event) {
  mouse_drag_active = true;
  last_mouse_x = event->globalX();
  last_mouse_y = event->globalY();
  grabMouse();
}

void GLView::mouseMoveEvent(QMouseEvent *event) {
  int this_mouse_x = event->globalX();
  int this_mouse_y = event->globalY();
  if (mouse_drag_active) {
    object_rot_y += (this_mouse_y - last_mouse_y) * 0.7;
    object_rot_z += (this_mouse_x - last_mouse_x) * 0.7;
    updateGL();
  }
  last_mouse_x = this_mouse_x;
  last_mouse_y = this_mouse_y;
}

void GLView::mouseReleaseEvent(QMouseEvent*) {
  mouse_drag_active = false;
  releaseMouse();
}

