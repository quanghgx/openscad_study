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

AbstractModule::~AbstractModule() {
}

AbstractNode *AbstractModule::evaluate(const Context*, const ModuleInstanciation *inst) const {
  AbstractNode *node = new AbstractNode(inst);

  foreach(ModuleInstanciation *v, inst->children) {
    AbstractNode *n = v->evaluate(inst->ctx);
    if (n)
      node->children.append(n);
  }

  return node;
}

QString AbstractModule::dump(QString indent, QString name) const {
  return QString("%1abstract module %2();\n").arg(indent, name);
}

ModuleInstanciation::~ModuleInstanciation() {
  foreach(Expression *v, argexpr)
          delete v;
  foreach(ModuleInstanciation *v, children)
          delete v;
}

QString ModuleInstanciation::dump(QString indent) const {
  QString text = indent;
  if (!label.isEmpty())
    text += label + QString(": ");
  text += modname + QString("(");
  for (int i = 0; i < argnames.size(); i++) {
    if (i > 0)
      text += QString(", ");
    if (!argnames[i].isEmpty())
      text += argnames[i] + QString(" = ");
    text += argexpr[i]->dump();
  }
  if (children.size() == 0) {
    text += QString(");\n");
  } else if (children.size() == 1) {
    text += QString(")\n");
    text += children[0]->dump(indent + QString("\t"));
  } else {
    text += QString(") {\n");
    for (int i = 0; i < children.size(); i++) {
      text += children[i]->dump(indent + QString("\t"));
    }
    text += QString("%1}\n").arg(indent);
  }
  return text;
}

AbstractNode *ModuleInstanciation::evaluate(const Context *ctx) const {
  AbstractNode *node = NULL;
  if (this->ctx) {
    PRINTA("WARNING: Ignoring recursive module instanciation of '%1'.", modname);
  } else {
    ModuleInstanciation *that = (ModuleInstanciation*)this;
    that->argvalues.clear();

    foreach(Expression *v, that->argexpr) {
      that->argvalues.append(v->evaluate(ctx));
    }
    that->ctx = ctx;
    node = ctx->evaluate_module(this);
    that->ctx = NULL;
    that->argvalues.clear();
  }
  return node;
}

Module::~Module() {
  foreach(Expression *v, assignments_expr)
          delete v;
  foreach(AbstractFunction *v, functions)
          delete v;
  foreach(AbstractModule *v, modules)
          delete v;
  foreach(ModuleInstanciation *v, children)
          delete v;
}

AbstractNode *Module::evaluate(const Context *ctx, const ModuleInstanciation *inst) const {
  Context c(ctx);
  c.args(argnames, argexpr, inst->argnames, inst->argvalues);

  c.functions_p = &functions;
  c.modules_p = &modules;

  for (int i = 0; i < assignments_var.size(); i++) {
    c.set_variable(assignments_var[i], assignments_expr[i]->evaluate(&c));
  }

  AbstractNode *node = new AbstractNode(inst);
  for (int i = 0; i < children.size(); i++) {
    AbstractNode *n = children[i]->evaluate(&c);
    if (n != NULL)
      node->children.append(n);
  }

  foreach(ModuleInstanciation *v, inst->children) {
    AbstractNode *n = v->evaluate(inst->ctx);
    if (n != NULL)
      node->children.append(n);
  }

  return node;
}

QString Module::dump(QString indent, QString name) const {
  QString text, tab;
  if (!name.isEmpty()) {
    text = QString("%1module %2(").arg(indent, name);
    for (int i = 0; i < argnames.size(); i++) {
      if (i > 0)
        text += QString(", ");
      text += argnames[i];
      if (argexpr[i])
        text += QString(" = ") + argexpr[i]->dump();
    }
    text += QString(") {\n");
    tab = "\t";
  }
  {
    QHashIterator<QString, AbstractFunction*> i(functions);
    while (i.hasNext()) {
      i.next();
      text += i.value()->dump(indent + tab, i.key());
    }
  }
  {
    QHashIterator<QString, AbstractModule*> i(modules);
    while (i.hasNext()) {
      i.next();
      text += i.value()->dump(indent + tab, i.key());
    }
  }
  for (int i = 0; i < assignments_var.size(); i++) {
    text += QString("%1%2 = %3;\n").arg(indent + tab, assignments_var[i], assignments_expr[i]->dump());
  }
  for (int i = 0; i < children.size(); i++) {
    text += children[i]->dump(indent + tab);
  }
  if (!name.isEmpty()) {
    text += QString("%1}\n").arg(indent);
  }
  return text;
}

QHash<QString, AbstractModule*> builtin_modules;

void initialize_builtin_modules() {
  builtin_modules["group"] = new AbstractModule();

  register_builtin_csgops();
  register_builtin_transform();
  register_builtin_primitives();
  register_builtin_surface();
  register_builtin_control();
  register_builtin_render();
  register_builtin_dxf_linear_extrude();
  register_builtin_dxf_rotate_extrude();
}

void destroy_builtin_modules() {
  foreach(AbstractModule *v, builtin_modules)
          delete v;
  builtin_modules.clear();
}

int AbstractNode::idx_counter;

AbstractNode::AbstractNode(const ModuleInstanciation *mi) {
  modinst = mi;
  idx = idx_counter++;
}

AbstractNode::~AbstractNode() {
  foreach(AbstractNode *v, children)
          delete v;
}

QString AbstractNode::mk_cache_id() const {
  QString cache_id = dump("");
  cache_id.remove(QRegExp("[a-zA-Z_][a-zA-Z_0-9]*:"));
  cache_id.remove(' ');
  cache_id.remove('\t');
  cache_id.remove('\n');
  return cache_id;
}


QCache<QString, CGAL_Nef_polyhedron> AbstractNode::cgal_nef_cache(100000);

CGAL_Nef_polyhedron AbstractNode::render_cgal_nef_polyhedron() const {
  QString cache_id = mk_cache_id();
  if (cgal_nef_cache.contains(cache_id)) {
    progress_report();
    return *cgal_nef_cache[cache_id];
  }

  CGAL_Nef_polyhedron N;

  foreach(AbstractNode *v, children) {
    if (v->modinst->tag_background)
      continue;
    N += v->render_cgal_nef_polyhedron();
  }

  cgal_nef_cache.insert(cache_id, new CGAL_Nef_polyhedron(N), N.number_of_vertices());
  progress_report();
  return N;
}


CSGTerm *AbstractNode::render_csg_term(double m[16], QVector<CSGTerm*> *highlights, QVector<CSGTerm*> *background) const {
  CSGTerm *t1 = NULL;

  foreach(AbstractNode *v, children) {
    CSGTerm *t2 = v->render_csg_term(m, highlights, background);
    if (t2 && !t1) {
      t1 = t2;
    } else if (t2 && t1) {
      t1 = new CSGTerm(CSGTerm::TYPE_UNION, t1, t2);
    }
  }
  if (t1 && modinst->tag_highlight && highlights)
    highlights->append(t1->link());
  if (t1 && modinst->tag_background && background) {
    background->append(t1);
    return NULL;
  }
  return t1;
}

QString AbstractNode::dump(QString indent) const {
  if (dump_cache.isEmpty()) {
    QString text = indent + QString("n%1: group() {\n").arg(idx);
    foreach(AbstractNode *v, children)
    text += v->dump(indent + QString("\t"));
    ((AbstractNode*)this)->dump_cache = text + indent + "}\n";
  }
  return dump_cache;
}

int progress_report_count;
void (*progress_report_f)(const class AbstractNode*, void*, int);
void *progress_report_vp;

void AbstractNode::progress_prepare() {
  foreach(AbstractNode *v, children)
  v->progress_prepare();
  progress_mark = ++progress_report_count;
}

void AbstractNode::progress_report() const {
  if (progress_report_f)
    progress_report_f(this, progress_report_vp, progress_mark);
}

void progress_report_prep(AbstractNode *root, void (*f)(const class AbstractNode *node, void *vp, int mark), void *vp) {
  progress_report_count = 0;
  progress_report_f = f;
  progress_report_vp = vp;
  root->progress_prepare();
}

void progress_report_fin() {
  progress_report_count = 0;
  progress_report_f = NULL;
  progress_report_vp = NULL;
}

