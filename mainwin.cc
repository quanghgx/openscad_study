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

#define INCLUDE_ABSTRACT_NODE_DETAILS

#include "openscad.h"

#include <QMenu>
#include <QTime>
#include <QMenuBar>
#include <QSplitter>
#include <QFileDialog>
#include <QApplication>
#include <QProgressDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>

//for chdir
#include <unistd.h>

QPointer<MainWindow> current_win;

MainWindow::MainWindow(const char *filename) {
  root_ctx.functions_p = &builtin_functions;
  root_ctx.modules_p = &builtin_modules;
  root_ctx.set_variable("$fn", Value(0.0));
  root_ctx.set_variable("$fs", Value(1.0));
  root_ctx.set_variable("$fa", Value(12.0));
  root_ctx.set_variable("$t", Value(0.0));

  root_module = NULL;
  absolute_root_node = NULL;
  root_raw_term = NULL;
  root_norm_term = NULL;
  root_chain = NULL;
  root_N = NULL;

  highlights_chain = NULL;
  background_chain = NULL;
  root_node = NULL;
  enableOpenCSG = false;

  tval = 0;
  fps = 0;
  fsteps = 1;

  s1 = new QSplitter(Qt::Horizontal, this);
  editor = new QTextEdit(s1);

  QWidget *w1 = new QWidget(s1);
  QVBoxLayout *l1 = new QVBoxLayout(w1);
  l1->setSpacing(0);
  l1->setMargin(0);

  s2 = new QSplitter(Qt::Vertical, w1);
  l1->addWidget(s2);
  screen = new GLView(s2);
  console = new QTextEdit(s2);

  QWidget *w2 = new QWidget(w1);
  QHBoxLayout *l2 = new QHBoxLayout(w2);
  l1->addWidget(w2);

  animate_timer = new QTimer(this);
  connect(animate_timer, SIGNAL(timeout()), this, SLOT(updateTVal()));

  l2->addWidget(new QLabel("Time:", w2));
  l2->addWidget(e_tval = new QLineEdit("0", w2));
  connect(e_tval, SIGNAL(textChanged(QString)), this, SLOT(actionCompile()));

  l2->addWidget(new QLabel("FPS:", w2));
  l2->addWidget(e_fps = new QLineEdit("0", w2));
  connect(e_fps, SIGNAL(textChanged(QString)), this, SLOT(updatedFps()));

  l2->addWidget(new QLabel("Steps:", w2));
  l2->addWidget(e_fsteps = new QLineEdit("100", w2));

  animate_panel = w2;
  animate_panel->hide();

  {
    QMenu *menu = menuBar()->addMenu("&File");
    menu->addAction("&New", this, SLOT(actionNew()));
    menu->addAction("&Open...", this, SLOT(actionOpen()));
    menu->addAction("&Save", this, SLOT(actionSave()), QKeySequence(Qt::Key_F2));
    menu->addAction("Save &As...", this, SLOT(actionSaveAs()));
    menu->addAction("&Reload", this, SLOT(actionReload()), QKeySequence(Qt::Key_F3));
    menu->addAction("&Quit", this, SLOT(close()));
  }

  {
    QMenu *menu = menuBar()->addMenu("&Edit");
    menu->addAction("&Undo", editor, SLOT(undo()), QKeySequence(Qt::CTRL + Qt::Key_Z));
    menu->addAction("&Redo", editor, SLOT(redo()), QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Z));
    menu->addSeparator();
    menu->addAction("Cu&t", editor, SLOT(cut()), QKeySequence(Qt::CTRL + Qt::Key_X));
    menu->addAction("&Copy", editor, SLOT(copy()), QKeySequence(Qt::CTRL + Qt::Key_C));
    menu->addAction("&Paste", editor, SLOT(paste()), QKeySequence(Qt::CTRL + Qt::Key_V));
    menu->addSeparator();
    menu->addAction("&Indent", this, SLOT(editIndent()), QKeySequence(Qt::CTRL + Qt::Key_I));
    menu->addAction("&Unindent", this, SLOT(editUnindent()), QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_I));
    menu->addSeparator();
    menu->addAction("C&omment", this, SLOT(editComment()), QKeySequence(Qt::CTRL + Qt::Key_D));
    menu->addAction("&Uncomment", this, SLOT(editUncomment()), QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D));
    menu->addSeparator();
    menu->addAction("Zoom In", editor, SLOT(zoomIn()), QKeySequence(Qt::CTRL + Qt::Key_Plus));
    menu->addAction("Zoom Out", editor, SLOT(zoomOut()), QKeySequence(Qt::CTRL + Qt::Key_Minus));
  }

  {
    QMenu *menu = menuBar()->addMenu("&Design");
    menu->addAction("&Reload and Compile", this, SLOT(actionReloadCompile()), QKeySequence(Qt::Key_F4));
    menu->addAction("&Compile", this, SLOT(actionCompile()), QKeySequence(Qt::Key_F5));
    menu->addAction("Compile and &Render (CGAL)", this, SLOT(actionRenderCGAL()), QKeySequence(Qt::Key_F6));
    menu->addAction("Display &AST...", this, SLOT(actionDisplayAST()));
    menu->addAction("Display CSG &Tree...", this, SLOT(actionDisplayCSGTree()));
    menu->addAction("Display CSG &Products...", this, SLOT(actionDisplayCSGProducts()));
    menu->addAction("Export as &STL...", this, SLOT(actionExportSTL()));
    menu->addAction("Export as &OFF...", this, SLOT(actionExportOFF()));
  }

  {
    QMenu *menu = menuBar()->addMenu("&View");
    actViewModeCGALSurface = menu->addAction("CGAL Surfaces", this, SLOT(viewModeCGALSurface()));
    actViewModeCGALGrid = menu->addAction("CGAL Grid Only", this, SLOT(viewModeCGALGrid()));
    actViewModeCGALSurface->setCheckable(true);
    actViewModeCGALGrid->setCheckable(true);
    actViewModeThrownTogether = menu->addAction("Thrown Together", this, SLOT(viewModeThrownTogether()));
    actViewModeThrownTogether->setCheckable(true);

    menu->addSeparator();
    actViewModeShowEdges = menu->addAction("Show Edges", this, SLOT(viewModeShowEdges()));
    actViewModeShowEdges->setCheckable(true);
    actViewModeAnimate = menu->addAction("Animate", this, SLOT(viewModeAnimate()));
    actViewModeAnimate->setCheckable(true);

    menu->addSeparator();
    menu->addAction("Top");
    menu->addAction("Bottom");
    menu->addAction("Left");
    menu->addAction("Right");
    menu->addAction("Front");
    menu->addAction("Back");
    menu->addAction("Diagonal");
    menu->addSeparator();
    menu->addAction("Perspective");
    menu->addAction("Orthogonal");
  }

  console->setReadOnly(true);
  current_win = this;

  PRINT("OpenSCAD (www.openscad.at)");
  PRINT("Copyright (C) 2009  Clifford Wolf <clifford@clifford.at>");
  PRINT("");
  PRINT("This program is free software; you can redistribute it and/or modify");
  PRINT("it under the terms of the GNU General Public License as published by");
  PRINT("the Free Software Foundation; either version 2 of the License, or");
  PRINT("(at your option) any later version.");
  PRINT("");

  editor->setTabStopWidth(30);

  if (filename) {
    this->filename = QString(filename);
    maybe_change_dir();
    setWindowTitle(this->filename);
    load();
  } else {
    setWindowTitle("New Document");
  }

  viewModeThrownTogether();

  setCentralWidget(s1);
  current_win = NULL;
}

MainWindow::~MainWindow() {
  if (root_module)
    delete root_module;
  if (root_node)
    delete root_node;
  if (root_N)
    delete root_N;
}

void MainWindow::updatedFps() {
  bool fps_ok;
  double fps = e_fps->text().toDouble(&fps_ok);
  if (!fps_ok || fps <= 0) {
    animate_timer->stop();
  } else {
    animate_timer->setInterval(int(1000 / e_fps->text().toDouble()));
    animate_timer->start();
  }
}

void MainWindow::updateTVal() {
  double s = e_fsteps->text().toDouble();
  double t = e_tval->text().toDouble() + 1 / s;
  QString txt;
  txt.sprintf("%.5f", t >= 1.0 ? 0.0 : t);
  e_tval->setText(txt);
}

void MainWindow::load() {
  if (!filename.isEmpty()) {
    QString text;
    FILE *fp = fopen(filename.toLatin1().data(), "rt");
    if (!fp) {
      PRINTA("Failed to open file: %1 (%2)", filename, QString(strerror(errno)));
    } else {
      char buffer[513];
      int rc;
      while ((rc = fread(buffer, 1, 512, fp)) > 0) {
        buffer[rc] = 0;
        text += buffer;
      }
      fclose(fp);
      PRINTA("Loaded design `%1'.", filename);
    }
    editor->setPlainText(text);
  }
}

void MainWindow::maybe_change_dir() {
  if (filename.isEmpty())
    return;

  int dir_end_index = filename.lastIndexOf("/");
  if (dir_end_index < 0)
    return;

  QString dirname = filename.mid(0, dir_end_index);
  if (chdir(dirname.toLatin1().data()) < 0)
    return;

  filename = filename.mid(dir_end_index + 1);
}

void MainWindow::find_root_tag(AbstractNode *n) {

  foreach(AbstractNode *v, n->children) {
    if (v->modinst->tag_root)
      root_node = v;
    if (root_node)
      return;
    find_root_tag(v);
  }
}

void MainWindow::compile(bool procevents) {
  PRINT("Parsing design (AST generation)...");
  if (procevents)
    QApplication::processEvents();

  if (root_module) {
    delete root_module;
    root_module = NULL;
  }

  if (absolute_root_node) {
    delete absolute_root_node;
    absolute_root_node = NULL;
  }

  if (root_raw_term) {
    root_raw_term->unlink();
    root_raw_term = NULL;
  }

  if (root_norm_term) {
    root_norm_term->unlink();
    root_norm_term = NULL;
  }

  if (root_chain) {
    delete root_chain;
    root_chain = NULL;
  }

  foreach(CSGTerm *v, highlight_terms) {
    v->unlink();
  }
  highlight_terms.clear();
  if (highlights_chain) {
    delete highlights_chain;
    highlights_chain = NULL;
  }

  foreach(CSGTerm *v, background_terms) {
    v->unlink();
  }
  background_terms.clear();
  if (background_chain) {
    delete background_chain;
    background_chain = NULL;
  }
  root_node = NULL;
  enableOpenCSG = false;

  root_ctx.set_variable("$t", Value(e_tval->text().toDouble()));
  root_module = parse(editor->toPlainText().toLatin1().data(), false);

  if (!root_module)
    goto fail;

  PRINT("Compiling design (CSG Tree generation)...");
  if (procevents)
    QApplication::processEvents();

  AbstractNode::idx_counter = 1;
  {
    ModuleInstanciation root_inst;
    absolute_root_node = root_module->evaluate(&root_ctx, &root_inst);
  }

  if (!absolute_root_node)
    goto fail;

  find_root_tag(absolute_root_node);
  if (!root_node)
    root_node = absolute_root_node;
  root_node->dump("");

  PRINT("Compiling design (CSG Products generation)...");
  if (procevents)
    QApplication::processEvents();

  double m[16];

  for (int i = 0; i < 16; i++)
    m[i] = i % 5 == 0 ? 1.0 : 0.0;

  root_raw_term = root_node->render_csg_term(m, &highlight_terms, &background_terms);

  if (!root_raw_term)
    goto fail;

  PRINT("Compiling design (CSG Products normalization)...");
  if (procevents)
    QApplication::processEvents();

  root_norm_term = root_raw_term->link();

  while (1) {
    CSGTerm *n = root_norm_term->normalize();
    root_norm_term->unlink();
    if (root_norm_term == n)
      break;
    root_norm_term = n;
  }

  if (!root_norm_term)
    goto fail;

  root_chain = new CSGChain();
  root_chain->import(root_norm_term);

  if (root_chain->polysets.size() > 1000) {
    PRINTF("WARNING: Normalized tree has %d elements!", root_chain->polysets.size());
    PRINTF("WARNING: OpenCSG rendering has been disabled.");
  } else {
    enableOpenCSG = true;
  }

  if (highlight_terms.size() > 0) {
    PRINTF("Compiling highlights (%d CSG Trees)...", highlight_terms.size());
    if (procevents)
      QApplication::processEvents();

    highlights_chain = new CSGChain();
    for (int i = 0; i < highlight_terms.size(); i++) {
      while (1) {
        CSGTerm *n = highlight_terms[i]->normalize();
        highlight_terms[i]->unlink();
        if (highlight_terms[i] == n)
          break;
        highlight_terms[i] = n;
      }
      highlights_chain->import(highlight_terms[i]);
    }
  }

  if (background_terms.size() > 0) {
    PRINTF("Compiling background (%d CSG Trees)...", background_terms.size());
    if (procevents)
      QApplication::processEvents();

    background_chain = new CSGChain();
    for (int i = 0; i < background_terms.size(); i++) {
      while (1) {
        CSGTerm *n = background_terms[i]->normalize();
        background_terms[i]->unlink();
        if (background_terms[i] == n)
          break;
        background_terms[i] = n;
      }
      background_chain->import(background_terms[i]);
    }
  }

  if (1) {
    PRINT("Compilation finished.");
    if (procevents)
      QApplication::processEvents();
  } else {
fail:
    PRINT("ERROR: Compilation failed!");
    if (procevents)
      QApplication::processEvents();
  }
}

void MainWindow::actionNew() {
  filename = QString();
  setWindowTitle("New Document");
  editor->setPlainText("");
}

void MainWindow::actionOpen() {
  current_win = this;
  QString new_filename = QFileDialog::getOpenFileName(this, "Open File", "", "OpenSCAD Designs (*.scad)");
  if (!new_filename.isEmpty()) {
    filename = new_filename;
    maybe_change_dir();
    setWindowTitle(filename);

    QString text;
    FILE *fp = fopen(filename.toLatin1().data(), "rt");
    if (!fp) {
      PRINTA("Failed to open file: %1 (%2)", QString(filename), QString(strerror(errno)));
    } else {
      char buffer[513];
      int rc;
      while ((rc = fread(buffer, 1, 512, fp)) > 0) {
        buffer[rc] = 0;
        text += buffer;
      }
      fclose(fp);
      PRINTA("Loaded design `%1'.", QString(filename));
    }
    editor->setPlainText(text);
  }
  current_win = NULL;
}

void MainWindow::actionSave() {
  current_win = this;
  FILE *fp = fopen(filename.toLatin1().data(), "wt");
  if (!fp) {
    PRINTA("Failed to open file for writing: %1 (%2)", QString(filename), QString(strerror(errno)));
  } else {
    fprintf(fp, "%s", editor->toPlainText().toLatin1().data());
    fclose(fp);
    PRINTA("Saved design `%1'.", QString(filename));
  }
  current_win = NULL;
}

void MainWindow::actionSaveAs() {
  QString new_filename = QFileDialog::getSaveFileName(this, "Save File", filename, "OpenSCAD Designs (*.scad)");
  if (!new_filename.isEmpty()) {
    filename = new_filename;
    maybe_change_dir();
    setWindowTitle(filename);
    actionSave();
  }
}

void MainWindow::actionReload() {
  current_win = this;
  load();
  current_win = NULL;
}

void MainWindow::editIndent() {
  QTextCursor cursor = editor->textCursor();
  int p1 = cursor.selectionStart();
  QString txt = cursor.selectedText();

  txt.replace(QString(QChar(8233)), QString(QChar(8233)) + QString("\t"));
  if (txt.endsWith(QString(QChar(8233)) + QString("\t")))
    txt.chop(1);
  txt = QString("\t") + txt;

  cursor.insertText(txt);
  int p2 = cursor.position();
  cursor.setPosition(p1, QTextCursor::MoveAnchor);
  cursor.setPosition(p2, QTextCursor::KeepAnchor);
  editor->setTextCursor(cursor);
}

void MainWindow::editUnindent() {
  QTextCursor cursor = editor->textCursor();
  int p1 = cursor.selectionStart();
  QString txt = cursor.selectedText();

  txt.replace(QString(QChar(8233)) + QString("\t"), QString(QChar(8233)));
  if (txt.startsWith(QString("\t")))
    txt.remove(0, 1);

  cursor.insertText(txt);
  int p2 = cursor.position();
  cursor.setPosition(p1, QTextCursor::MoveAnchor);
  cursor.setPosition(p2, QTextCursor::KeepAnchor);
  editor->setTextCursor(cursor);
}

void MainWindow::editComment() {
  QTextCursor cursor = editor->textCursor();
  int p1 = cursor.selectionStart();
  QString txt = cursor.selectedText();

  txt.replace(QString(QChar(8233)), QString(QChar(8233)) + QString("//"));
  if (txt.endsWith(QString(QChar(8233)) + QString("//")))
    txt.chop(2);
  txt = QString("//") + txt;

  cursor.insertText(txt);
  int p2 = cursor.position();
  cursor.setPosition(p1, QTextCursor::MoveAnchor);
  cursor.setPosition(p2, QTextCursor::KeepAnchor);
  editor->setTextCursor(cursor);
}

void MainWindow::editUncomment() {
  QTextCursor cursor = editor->textCursor();
  int p1 = cursor.selectionStart();
  QString txt = cursor.selectedText();

  txt.replace(QString(QChar(8233)) + QString("//"), QString(QChar(8233)));
  if (txt.startsWith(QString("//")))
    txt.remove(0, 2);

  cursor.insertText(txt);
  int p2 = cursor.position();
  cursor.setPosition(p1, QTextCursor::MoveAnchor);
  cursor.setPosition(p2, QTextCursor::KeepAnchor);
  editor->setTextCursor(cursor);
}

void MainWindow::actionReloadCompile() {
  current_win = this;
  console->clear();

  load();
  compile(true);

  {
    screen->updateGL();
  }
  current_win = NULL;
}

void MainWindow::actionCompile() {
  current_win = this;
  console->clear();

  compile(!actViewModeAnimate->isChecked());

  {
    screen->updateGL();
  }
  current_win = NULL;
}


static void report_func(const class AbstractNode*, void *vp, int mark) {
  QProgressDialog *pd = (QProgressDialog*) vp;
  int v = (int) ((mark * 100.0) / progress_report_count);
  pd->setValue(v < 100 ? v : 99);
  QString label;
  label.sprintf("Rendering Polygon Mesh using CGAL (%d/%d)", mark, progress_report_count);
  pd->setLabelText(label);
  QApplication::processEvents();
}

void MainWindow::actionRenderCGAL() {
  current_win = this;
  console->clear();

  compile(true);

  if (!root_module || !root_node)
    return;

  if (root_N) {
    delete root_N;
    root_N = NULL;
  }

  PRINT("Rendering Polygon Mesh using CGAL...");
  QApplication::processEvents();

  QTime t;
  t.start();

  QProgressDialog *pd = new QProgressDialog("Rendering Polygon Mesh using CGAL...", QString(), 0, 100);
  pd->setValue(0);
  pd->setAutoClose(false);
  pd->show();
  QApplication::processEvents();

  progress_report_prep(root_node, report_func, pd);
  root_N = new CGAL_Nef_polyhedron(root_node->render_cgal_nef_polyhedron());
  progress_report_fin();

  PRINTF("Number of vertices currently in CGAL cache: %d", AbstractNode::cgal_nef_cache.totalCost());
  PRINTF("Number of objects currently in CGAL cache: %d", AbstractNode::cgal_nef_cache.size());
  QApplication::processEvents();

  PRINTF("   Simple:     %6s", root_N->is_simple() ? "yes" : "no");
  QApplication::processEvents();
  PRINTF("   Valid:      %6s", root_N->is_valid() ? "yes" : "no");
  QApplication::processEvents();
  PRINTF("   Vertices:   %6d", (int) root_N->number_of_vertices());
  QApplication::processEvents();
  PRINTF("   Halfedges:  %6d", (int) root_N->number_of_halfedges());
  QApplication::processEvents();
  PRINTF("   Edges:      %6d", (int) root_N->number_of_edges());
  QApplication::processEvents();
  PRINTF("   Halffacets: %6d", (int) root_N->number_of_halffacets());
  QApplication::processEvents();
  PRINTF("   Facets:     %6d", (int) root_N->number_of_facets());
  QApplication::processEvents();
  PRINTF("   Volumes:    %6d", (int) root_N->number_of_volumes());
  QApplication::processEvents();

  int s = t.elapsed() / 1000;
  PRINTF("Total rendering time: %d hours, %d minutes, %d seconds", s / (60 * 60), (s / 60) % 60, s % 60);

  if (!actViewModeCGALSurface->isChecked() && !actViewModeCGALGrid->isChecked()) {
    viewModeCGALSurface();
  } else {
    screen->updateGL();
  }

  PRINT("Rendering finished.");

  delete pd;
  current_win = NULL;

}


void MainWindow::actionDisplayAST() {
  current_win = this;
  QTextEdit *e = new QTextEdit(NULL);
  e->setTabStopWidth(30);
  e->setWindowTitle("AST Dump");
  if (root_module) {
    e->setPlainText(root_module->dump("", ""));
  } else {
    e->setPlainText("No AST to dump. Please try compiling first...");
  }
  e->show();
  e->resize(600, 400);
  current_win = NULL;
}

void MainWindow::actionDisplayCSGTree() {
  current_win = this;
  QTextEdit *e = new QTextEdit(NULL);
  e->setTabStopWidth(30);
  e->setWindowTitle("CSG Tree Dump");
  if (root_node) {
    e->setPlainText(root_node->dump(""));
  } else {
    e->setPlainText("No CSG to dump. Please try compiling first...");
  }
  e->show();
  e->resize(600, 400);
  current_win = NULL;
}

void MainWindow::actionDisplayCSGProducts() {
  current_win = this;
  QTextEdit *e = new QTextEdit(NULL);
  e->setTabStopWidth(30);
  e->setWindowTitle("CSG Products Dump");
  e->setPlainText(QString("\nCSG before normalization:\n%1\n\n\nCSG after normalization:\n%2\n\n\nCSG rendering chain:\n%3\n\n\nHighlights CSG rendering chain:\n%4\n\n\nBackground CSG rendering chain:\n%5\n").arg(root_raw_term ? root_raw_term->dump() : "N/A", root_norm_term ? root_norm_term->dump() : "N/A", root_chain ? root_chain->dump() : "N/A", highlights_chain ? highlights_chain->dump() : "N/A", background_chain ? background_chain->dump() : "N/A"));
  e->show();
  e->resize(600, 400);
  current_win = NULL;
}

void MainWindow::actionExportSTL() {
  current_win = this;

  if (!root_N) {
    PRINT("Nothing to export! Try building first (press F6).");
    current_win = NULL;
    return;
  }

  if (!root_N->is_simple()) {
    PRINT("Object isn't a single polyeder or otherwise invalid! Modify your design..");
    current_win = NULL;
    return;
  }

  QString stl_filename = QFileDialog::getSaveFileName(this, "Export STL File", "", "STL Files (*.stl)");
  if (stl_filename.isEmpty()) {
    PRINT("No filename specified. STL export aborted.");
    current_win = NULL;
    return;
  }

  CGAL_Polyhedron P;
  root_N->convert_to_Polyhedron(P);

  typedef CGAL_Polyhedron::Vertex Vertex;
  typedef CGAL_Polyhedron::Vertex_const_iterator VCI;
  typedef CGAL_Polyhedron::Facet_const_iterator FCI;
  typedef CGAL_Polyhedron::Halfedge_around_facet_const_circulator HFCC;

  FILE *f = fopen(stl_filename.toLatin1().data(), "w");
  if (!f) {
    PRINT("Can't open STL file for STL export.");
    current_win = NULL;
    return;
  }
  fprintf(f, "solid\n");

  QProgressDialog *pd = new QProgressDialog("Exporting object to STL file...",
          QString(), 0, root_N->number_of_facets() + 1);
  pd->setValue(0);
  pd->setAutoClose(false);
  pd->show();
  QApplication::processEvents();

  int facet_count = 0;
  for (FCI fi = P.facets_begin(); fi != P.facets_end(); ++fi) {
    HFCC hc = fi->facet_begin();
    HFCC hc_end = hc;
    Vertex v1, v2, v3;
    v1 = *VCI((hc++)->vertex());
    v3 = *VCI((hc++)->vertex());
    do {
      v2 = v3;
      v3 = *VCI((hc++)->vertex());
      double x1 = CGAL::to_double(v1.point().x());
      double y1 = CGAL::to_double(v1.point().y());
      double z1 = CGAL::to_double(v1.point().z());
      double x2 = CGAL::to_double(v2.point().x());
      double y2 = CGAL::to_double(v2.point().y());
      double z2 = CGAL::to_double(v2.point().z());
      double x3 = CGAL::to_double(v3.point().x());
      double y3 = CGAL::to_double(v3.point().y());
      double z3 = CGAL::to_double(v3.point().z());
      QString vs1, vs2, vs3;
      vs1.sprintf("%f %f %f", x1, y1, z1);
      vs2.sprintf("%f %f %f", x2, y2, z2);
      vs3.sprintf("%f %f %f", x3, y3, z3);
      if (vs1 != vs2 && vs1 != vs3 && vs2 != vs3) {

        double nx = (y1 - y2)*(z1 - z3) - (z1 - z2)*(y1 - y3);
        double ny = (z1 - z2)*(x1 - x3) - (x1 - x2)*(z1 - z3);
        double nz = (x1 - x2)*(y1 - y3) - (y1 - y2)*(x1 - x3);
        double n_scale = 1 / sqrt(nx * nx + ny * ny + nz * nz);
        fprintf(f, "  facet normal %f %f %f\n",
                nx * n_scale, ny * n_scale, nz * n_scale);
        fprintf(f, "    outer loop\n");
        fprintf(f, "      vertex %s\n", vs1.toLatin1().data());
        fprintf(f, "      vertex %s\n", vs2.toLatin1().data());
        fprintf(f, "      vertex %s\n", vs3.toLatin1().data());
        fprintf(f, "    endloop\n");
        fprintf(f, "  endfacet\n");
      }
    } while (hc != hc_end);
    pd->setValue(facet_count++);
    QApplication::processEvents();
  }

  fprintf(f, "endsolid\n");
  fclose(f);

  PRINT("STL export finished.");

  delete pd;
  current_win = NULL;
}

void MainWindow::actionExportOFF() {
  current_win = this;
  PRINTA("Function %1 is not implemented yet!", QString(__PRETTY_FUNCTION__));
  current_win = NULL;
}

void MainWindow::viewModeActionsUncheck() {
  actViewModeCGALSurface->setChecked(false);
  actViewModeCGALGrid->setChecked(false);
  actViewModeThrownTogether->setChecked(false);
}



// a little hackish: we need access to default-private members of
// CGAL::OGL::Nef3_Converter so we can implement our own draw function
// that does not scale the model. so we define 'class' to 'struct'
// for this header..
//
// theoretically there could be two problems:
// 1.) defining language keyword with the pre processor is illegal afair
// 2.) the compiler could use a different memory layout or name mangling for structs
//
// both does not seam to be the case with todays compilers...
//
#define class struct
#include <CGAL/Nef_3/OGL_helper.h>
#undef class

static void renderGLviaCGAL(void *vp) {
  MainWindow *m = (MainWindow*) vp;
  if (m->root_N) {
    CGAL::OGL::Polyhedron P;
    CGAL::OGL::Nef3_Converter<CGAL_Nef_polyhedron>::convert_to_OGLPolyhedron(*m->root_N, &P);
    P.init();
    if (m->actViewModeCGALSurface->isChecked())
      P.set_style(CGAL::OGL::SNC_BOUNDARY);
    if (m->actViewModeCGALGrid->isChecked())
      P.set_style(CGAL::OGL::SNC_SKELETON);
#if 0
    P.draw();
#else
    if (P.style == CGAL::OGL::SNC_BOUNDARY) {
      glCallList(P.object_list_ + 2);
      if (m->actViewModeShowEdges->isChecked()) {
        glDisable(GL_LIGHTING);
        glCallList(P.object_list_ + 1);
        glCallList(P.object_list_);
      }
    } else {
      glDisable(GL_LIGHTING);
      glCallList(P.object_list_ + 1);
      glCallList(P.object_list_);
    }
#endif
  }
}

void MainWindow::viewModeCGALSurface() {
  viewModeActionsUncheck();
  actViewModeCGALSurface->setChecked(true);
  screen->renderfunc = renderGLviaCGAL;
  screen->renderfunc_vp = this;
  screen->updateGL();
}

void MainWindow::viewModeCGALGrid() {
  viewModeActionsUncheck();
  actViewModeCGALGrid->setChecked(true);
  screen->renderfunc = renderGLviaCGAL;
  screen->renderfunc_vp = this;
  screen->updateGL();
}


static void renderGLThrownTogetherChain(MainWindow *m, CSGChain *chain, bool highlight, bool background) {
  glDepthFunc(GL_LEQUAL);
  QHash<QPair<PolySet*, double*>, int> polySetVisitMark;
  bool showEdges = m->actViewModeShowEdges->isChecked();
  for (int i = 0; i < chain->polysets.size(); i++) {
    if (polySetVisitMark[QPair<PolySet*, double*>(chain->polysets[i], chain->matrices[i])]++ > 0)
      continue;
    glPushMatrix();
    glMultMatrixd(chain->matrices[i]);
    if (highlight) {
      chain->polysets[i]->render_surface(PolySet::COLORMODE_HIGHLIGHT);
      if (showEdges) {
        glDisable(GL_LIGHTING);
        chain->polysets[i]->render_edges(PolySet::COLORMODE_HIGHLIGHT);
        glEnable(GL_LIGHTING);
      }
    } else if (background) {
      chain->polysets[i]->render_surface(PolySet::COLORMODE_BACKGROUND);
      if (showEdges) {
        glDisable(GL_LIGHTING);
        chain->polysets[i]->render_edges(PolySet::COLORMODE_BACKGROUND);
        glEnable(GL_LIGHTING);
      }
    } else if (chain->types[i] == CSGTerm::TYPE_DIFFERENCE) {
      chain->polysets[i]->render_surface(PolySet::COLORMODE_CUTOUT);
      if (showEdges) {
        glDisable(GL_LIGHTING);
        chain->polysets[i]->render_edges(PolySet::COLORMODE_CUTOUT);
        glEnable(GL_LIGHTING);
      }
    } else {
      chain->polysets[i]->render_surface(PolySet::COLORMODE_MATERIAL);
      if (showEdges) {
        glDisable(GL_LIGHTING);
        chain->polysets[i]->render_edges(PolySet::COLORMODE_MATERIAL);
        glEnable(GL_LIGHTING);
      }
    }
    glPopMatrix();
  }
}

static void renderGLThrownTogether(void *vp) {
  MainWindow *m = (MainWindow*) vp;
  if (m->root_chain)
    renderGLThrownTogetherChain(m, m->root_chain, false, false);
  if (m->background_chain)
    renderGLThrownTogetherChain(m, m->background_chain, false, true);
  if (m->highlights_chain)
    renderGLThrownTogetherChain(m, m->highlights_chain, true, false);
}

void MainWindow::viewModeThrownTogether() {
  viewModeActionsUncheck();
  actViewModeThrownTogether->setChecked(true);
  screen->renderfunc = renderGLThrownTogether;
  screen->renderfunc_vp = this;
  screen->updateGL();
}

void MainWindow::viewModeShowEdges() {
  screen->updateGL();
}

void MainWindow::viewModeAnimate() {
  if (actViewModeAnimate->isChecked()) {
    animate_panel->show();
    updatedFps();
  } else {
    animate_panel->hide();
    animate_timer->stop();
  }
}

