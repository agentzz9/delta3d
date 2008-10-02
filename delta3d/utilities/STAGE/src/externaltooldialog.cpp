#include <dtEditQt/externaltooldialog.h>
#include <QtGui/QMessageBox>
#include <QtGui/QFileDialog>
#include <cassert>

using namespace dtEditQt;

dtEditQt::ExternalToolDialog::ExternalToolDialog(QList<ExternalTool*> &tools, QWidget* parent)
: QDialog(parent)
, mTools(&tools)
{
   ui.setupUi(this);
   
   ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
   ui.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

   SetupConnections();
   PopulateToolsUI();
}

dtEditQt::ExternalToolDialog::~ExternalToolDialog()
{
   while (ui.toolList->count() > 0)
   {
      QListWidgetItem* item = ui.toolList->takeItem(0);
      delete item;
   }
}

void dtEditQt::ExternalToolDialog::SetupConnections()
{
   connect(ui.addButton, SIGNAL(clicked()), this, SLOT(OnNewTool()));
   connect(ui.deleteButton, SIGNAL(clicked()), this, SLOT(OnRemoveTool()));
   connect(ui.toolList, SIGNAL(itemSelectionChanged()), this, SLOT(OnToolSelectionChanged()));
   connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(OnApplyChanges()));
   connect(ui.titleEdit, SIGNAL(textEdited(const QString&)), this, SLOT(OnStringChanged(const QString&)));
   connect(ui.commandEdit, SIGNAL(textEdited(const QString&)), this, SLOT(OnStringChanged(const QString&)));
   connect(ui.commandButton, SIGNAL(clicked()), this, SLOT(OnFindCommandFile()));
   connect(ui.argsEdit, SIGNAL(textEdited(const QString&)), this, SLOT(OnStringChanged(const QString&)));
   connect(ui.workingDirEdit, SIGNAL(textEdited(const QString&)), this, SLOT(OnStringChanged(const QString&)));
   connect(ui.workingDirButton, SIGNAL(clicked()), this, SLOT(OnFindWorkingDir()));
}


//////////////////////////////////////////////////////////////////////////
void dtEditQt::ExternalToolDialog::OnNewTool()
{
   ExternalTool *tool = NULL;

   //find the first ExternalTool that has a non-visible QAction.  We'll edit that one.
   for (int toolIdx=0; toolIdx<mTools->size(); toolIdx++)
   {
      if (mTools->at(toolIdx)->GetAction()->isVisible() == false)
      {
         tool = mTools->at(toolIdx);
         break;
      }
   }

   if (tool == NULL)
   {
      //no ExternalTools are available for editing.  We must have reached
      //the max.
      QMessageBox::information(this, tr("Tools"), 
                              tr("Maximum number of external tools reached.  Try deleting tools not used."));
      return;
   }

   assert(tool);   

   //mTools->push_back(tool);
   tool->GetAction()->setVisible(true);

   QListWidgetItem *item = new QListWidgetItem(tool->GetTitle(), ui.toolList);   
   ui.toolList->setCurrentItem(item); //make it the currently selected item
   
   SetOkButtonEnabled(true);
   SetModifyButtonsEnabled(false);
}

//////////////////////////////////////////////////////////////////////////
void dtEditQt::ExternalToolDialog::OnRemoveTool()
{
   if (ui.toolList->currentItem() == NULL)
   {
      //nothing selected
      return;
   }

   ExternalTool* tool = GetSelectedTool();
   if (tool == NULL)
   {
      return;
   }

   ui.toolList->takeItem(ui.toolList->currentRow()); //remove it's widget
   tool->GetAction()->setVisible(false); //turn off the QAction
   SetOkButtonEnabled(true);
   
   //now select something that still exists.
   if (ui.toolList->count() > 0)
   {
      ui.toolList->setCurrentItem(ui.toolList->item(0));
   }
}

//////////////////////////////////////////////////////////////////////////
void dtEditQt::ExternalToolDialog::PopulateToolsUI()
{
   for (int toolIdx=0; toolIdx<mTools->size(); toolIdx++)
   {
      //if the QAction is visible, that means its already been created and ready to edit.
      //Otherwise, don't create an item for the ExternalTool
      if (mTools->at(toolIdx)->GetAction()->isVisible())
      {
         ui.toolList->addItem(mTools->at(toolIdx)->GetTitle());
      }
   }

   //now select something that still exists.
   if (ui.toolList->count() > 0)
   {
      ui.toolList->setCurrentItem(ui.toolList->item(0));
   }
}

//////////////////////////////////////////////////////////////////////////
void dtEditQt::ExternalToolDialog::OnToolSelectionChanged()
{
   const ExternalTool* tool = GetSelectedTool();
   if (tool == NULL)
   { 
      ui.titleEdit->setText(QString());
      ui.commandEdit->setText(QString());
      return;
   }

   ui.titleEdit->setText(tool->GetTitle());
   ui.commandEdit->setText(tool->GetCmd());
   ui.argsEdit->setText(tool->GetArgs());
   ui.workingDirEdit->setText(tool->GetWorkingDir());
}

//////////////////////////////////////////////////////////////////////////
ExternalTool* dtEditQt::ExternalToolDialog::GetSelectedTool() const
{
   const QListWidgetItem *currentItem = ui.toolList->currentItem();
   if (currentItem == NULL)
   {
      //nothing selected
      return NULL;
   }

   ExternalTool* tool = NULL;
   for (int i=0; i<mTools->size(); ++i)
   {
      if (mTools->at(i)->GetTitle() == currentItem->text())
      {
         tool = mTools->at(i);
         break;
      }
   }

   return tool;
}

//////////////////////////////////////////////////////////////////////////
void dtEditQt::ExternalToolDialog::OnApplyChanges()
{
   QListWidgetItem *currentItem = ui.toolList->currentItem();
   if (currentItem == NULL)
   {
      //nothing selected
      assert(currentItem);
      return;
   }

   ExternalTool* tool = GetSelectedTool();
   if (tool == NULL)
   {
      assert(tool);
      return;
   }

   currentItem->setText(ui.titleEdit->text());
   tool->SetTitle(ui.titleEdit->text());
   tool->SetCmd(ui.commandEdit->text());
   tool->SetArgs(ui.argsEdit->text());
   tool->SetWorkingDir(ui.workingDirEdit->text());

   ui.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
   
   SetModifyButtonsEnabled(true);
}

//////////////////////////////////////////////////////////////////////////
void dtEditQt::ExternalToolDialog::SetOkButtonEnabled(bool enabled)
{
   ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enabled);
   ui.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(enabled);
}

//////////////////////////////////////////////////////////////////////////
void dtEditQt::ExternalToolDialog::OnToolModified()
{
   SetOkButtonEnabled(true);
   SetModifyButtonsEnabled(false);
}

//////////////////////////////////////////////////////////////////////////
void dtEditQt::ExternalToolDialog::OnStringChanged(const QString& text)
{
   OnToolModified();
}

//////////////////////////////////////////////////////////////////////////
void dtEditQt::ExternalToolDialog::accept()
{
   if (ui.buttonBox->button(QDialogButtonBox::Apply)->isEnabled())
   {
      OnApplyChanges();
   }
   
   QDialog::accept();
}

//////////////////////////////////////////////////////////////////////////
void dtEditQt::ExternalToolDialog::SetModifyButtonsEnabled(bool enabled)
{
   ui.addButton->setEnabled(enabled);
   ui.deleteButton->setEnabled(enabled);
   ui.moveDownButton->setEnabled(enabled);
   ui.moveUpButton->setEnabled(enabled);
}

//////////////////////////////////////////////////////////////////////////
void dtEditQt::ExternalToolDialog::OnFindCommandFile()
{
   ExternalTool* tool = GetSelectedTool();

   if (tool == NULL)
   {
      return;
   }

   //pop open a file dialog and query for a filename
   const QString filename = QFileDialog::getOpenFileName(this,
                     tr("Get File"),
                     QFileInfo(tool->GetCmd()).path());

   if (!filename.isEmpty())
   {
      tool->SetCmd(filename);
      ui.commandEdit->setText(filename);
      OnToolModified();
   }
}

//////////////////////////////////////////////////////////////////////////
void dtEditQt::ExternalToolDialog::OnFindWorkingDir()
{
   ExternalTool* tool = GetSelectedTool();

   if (tool == NULL)
   {
      return;
   }

   //pop open a file dialog and query for a workingDir
   const QString workingDir = QFileDialog::getExistingDirectory(this,
                              tr("Get Directory"),
                              tool->GetWorkingDir());

   if (!workingDir.isEmpty())
   {
      tool->SetWorkingDir(workingDir);
      ui.workingDirEdit->setText(workingDir);
      OnToolModified();
   }
}
