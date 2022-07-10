#include "screenplay_text_manager.h"

#include "screenplay_text_view.h"

#include <business_layer/model/screenplay/screenplay_dictionaries_model.h>
#include <business_layer/model/screenplay/text/screenplay_text_model.h>
#include <business_layer/model/text/text_model_text_item.h>
#include <data_layer/storage/settings_storage.h>
#include <data_layer/storage/storage_facade.h>
#include <domain/document_object.h>
#include <ui/dictionaries_view.h>
#include <ui/modules/bookmarks/bookmark_dialog.h>

#include <QApplication>
#include <QFileDialog>
#include <QStringListModel>


namespace ManagementLayer {

namespace {

const int kSceneIntrosIndex = 0;
const int kSceneTimesIndex = 1;
const int kCharacterExtensionsIndex = 2;
const int kTransitionIndex = 3;

const QString kSettingsKey = "screenplay-text";
QString cursorPositionFor(Domain::DocumentObject* _item)
{
    return QString("%1/%2/last-cursor").arg(kSettingsKey, _item->uuid().toString());
}
QString verticalScrollFor(Domain::DocumentObject* _item)
{
    return QString("%1/%2/vertical-scroll").arg(kSettingsKey, _item->uuid().toString());
}

} // namespace

class ScreenplayTextManager::Implementation
{
public:
    explicit Implementation(ScreenplayTextManager* _q);

    /**
     * @brief Создать представление
     */
    Ui::ScreenplayTextView* createView(BusinessLayer::AbstractModel* _model);

    /**
     * @brief Связать заданную модель и представление
     */
    void setModelForView(BusinessLayer::AbstractModel* _model, Ui::ScreenplayTextView* _view);


    /**
     * @brief Получить модель связанную с заданным представлением
     */
    QPointer<BusinessLayer::ScreenplayTextModel> modelForView(Ui::ScreenplayTextView* _view) const;


    /**
     * @brief Работа с параметрами отображения представления
     */
    void loadModelAndViewSettings(BusinessLayer::AbstractModel* _model,
                                  Ui::ScreenplayTextView* _view);
    void saveModelAndViewSettings(BusinessLayer::AbstractModel* _model,
                                  Ui::ScreenplayTextView* _view);

    /**
     * @brief Обновить переводы
     */
    void updateTranslations();

    /**
     * @brief Обновить список элементов словаря в редакторе словарей
     */
    void updateDictionaryItemsList(int _dictionaryType, Ui::ScreenplayTextView* _view);


    ScreenplayTextManager* q = nullptr;

    /**
     * @brief Модель типов справочников
     */
    QStringListModel* dictionariesTypesModel = nullptr;

    /**
     * @brief Элементы выбранного пользователем справочника
     */
    QStringListModel* dictionaryItemsModel = nullptr;

    /**
     * @brief Предаставление для основного окна
     */
    Ui::ScreenplayTextView* view = nullptr;
    Ui::ScreenplayTextView* secondaryView = nullptr;

    /**
     * @brief Все созданные представления с моделями, которые в них отображаются
     */
    struct ViewAndModel {
        QPointer<Ui::ScreenplayTextView> view;
        QPointer<BusinessLayer::ScreenplayTextModel> model;
    };
    QVector<ViewAndModel> allViews;
};

ScreenplayTextManager::Implementation::Implementation(ScreenplayTextManager* _q)
    : q(_q)
{
    dictionariesTypesModel = new QStringListModel(view);
    dictionaryItemsModel = new QStringListModel(view);
}

Ui::ScreenplayTextView* ScreenplayTextManager::Implementation::createView(
    BusinessLayer::AbstractModel* _model)
{
    auto view = new Ui::ScreenplayTextView;
    view->installEventFilter(q);
    view->dictionariesView()->setTypes(dictionariesTypesModel);
    view->dictionariesView()->setDictionaryItems(dictionaryItemsModel);
    setModelForView(_model, view);

    auto showBookmarkDialog = [this, view](Ui::BookmarkDialog::DialogType _type) {
        auto item = modelForView(view)->itemForIndex(view->currentModelIndex());
        if (item->type() != BusinessLayer::TextModelItemType::Text) {
            return;
        }

        auto dialog = new Ui::BookmarkDialog(view->topLevelWidget());
        dialog->setDialogType(_type);
        if (_type == Ui::BookmarkDialog::DialogType::Edit) {
            const auto textItem = static_cast<BusinessLayer::TextModelTextItem*>(item);
            dialog->setBookmarkName(textItem->bookmark()->name);
            dialog->setBookmarkColor(textItem->bookmark()->color);
        }
        connect(dialog, &Ui::BookmarkDialog::savePressed, q, [this, view, item, dialog] {
            auto textItem = static_cast<BusinessLayer::TextModelTextItem*>(item);
            textItem->setBookmark({ dialog->bookmarkColor(), dialog->bookmarkName() });
            modelForView(view)->updateItem(textItem);

            dialog->hideDialog();
        });
        connect(dialog, &Ui::BookmarkDialog::disappeared, dialog, &Ui::BookmarkDialog::deleteLater);

        //
        // Отображаем диалог
        //
        dialog->showDialog();
    };
    connect(view, &Ui::ScreenplayTextView::addBookmarkRequested, q, [showBookmarkDialog] {
        showBookmarkDialog(Ui::BookmarkDialog::DialogType::CreateNew);
    });
    connect(view, &Ui::ScreenplayTextView::editBookmarkRequested, q,
            [showBookmarkDialog] { showBookmarkDialog(Ui::BookmarkDialog::DialogType::Edit); });
    connect(view, &Ui::ScreenplayTextView::createBookmarkRequested, q,
            [this, view](const QString& _text, const QColor& _color) {
                auto item = modelForView(view)->itemForIndex(view->currentModelIndex());
                if (item->type() != BusinessLayer::TextModelItemType::Text) {
                    return;
                }

                auto textItem = static_cast<BusinessLayer::TextModelTextItem*>(item);
                textItem->setBookmark({ _color, _text });
                modelForView(view)->updateItem(textItem);
            });
    connect(view, &Ui::ScreenplayTextView::changeBookmarkRequested, q,
            [this, view](const QModelIndex& _index, const QString& _text, const QColor& _color) {
                auto item = modelForView(view)->itemForIndex(_index);
                if (item->type() != BusinessLayer::TextModelItemType::Text) {
                    return;
                }

                auto textItem = static_cast<BusinessLayer::TextModelTextItem*>(item);
                textItem->setBookmark({ _color, _text });
                modelForView(view)->updateItem(textItem);
            });
    connect(view, &Ui::ScreenplayTextView::removeBookmarkRequested, q, [this, view] {
        auto item = modelForView(view)->itemForIndex(view->currentModelIndex());
        if (item->type() != BusinessLayer::TextModelItemType::Text) {
            return;
        }

        auto textItem = static_cast<BusinessLayer::TextModelTextItem*>(item);
        textItem->clearBookmark();
        modelForView(view)->updateItem(textItem);
    });
    //
    connect(
        view->dictionariesView(), &Ui::DictionariesView::typeChanged, q,
        [this, view](const QModelIndex& _index) { updateDictionaryItemsList(_index.row(), view); });
    connect(view->dictionariesView(), &Ui::DictionariesView::addItemRequested, q,
            [this, view](const QModelIndex& _typeIndex) {
                auto model = modelForView(view);
                if (model == nullptr) {
                    return;
                }

                auto dictionaries = model->dictionariesModel();
                Q_ASSERT(dictionaries);
                switch (_typeIndex.row()) {
                case kSceneIntrosIndex: {
                    dictionaries->addSceneIntro("");
                    break;
                }

                case kSceneTimesIndex: {
                    dictionaries->addSceneTime("");
                    break;
                }

                case kCharacterExtensionsIndex: {
                    dictionaries->addCharacterExtension("");
                    break;
                }

                case kTransitionIndex: {
                    dictionaries->addTransition("");
                    break;
                }
                }

                view->dictionariesView()->editLastItem();
            });
    connect(view->dictionariesView(), &Ui::DictionariesView::editItemRequested, q,
            [this, view](const QModelIndex& _typeIndex, const QModelIndex& _itemIndex,
                         const QString& _item) {
                auto model = modelForView(view);
                if (modelForView(view) == nullptr) {
                    return;
                }

                auto dictionaries = model->dictionariesModel();
                Q_ASSERT(dictionaries);
                switch (_typeIndex.row()) {
                case kSceneIntrosIndex: {
                    dictionaries->setSceneIntro(_itemIndex.row(), _item);
                    break;
                }

                case kSceneTimesIndex: {
                    dictionaries->setSceneTime(_itemIndex.row(), _item);
                    break;
                }

                case kCharacterExtensionsIndex: {
                    dictionaries->setCharacterExtension(_itemIndex.row(), _item);
                    break;
                }

                case kTransitionIndex: {
                    dictionaries->setTransition(_itemIndex.row(), _item);
                    break;
                }
                }
            });
    connect(view->dictionariesView(), &Ui::DictionariesView::removeItemRequested, q,
            [this, view](const QModelIndex& _typeIndex, const QModelIndex& _itemIndex) {
                auto model = modelForView(view);
                if (modelForView(view) == nullptr) {
                    return;
                }

                auto dictionaries = model->dictionariesModel();
                Q_ASSERT(dictionaries);
                switch (_typeIndex.row()) {
                case kSceneIntrosIndex: {
                    dictionaries->removeSceneIntro(_itemIndex.row());
                    break;
                }

                case kSceneTimesIndex: {
                    dictionaries->removeSceneTime(_itemIndex.row());
                    break;
                }

                case kCharacterExtensionsIndex: {
                    dictionaries->removeCharacterExtension(_itemIndex.row());
                    break;
                }

                case kTransitionIndex: {
                    dictionaries->removeTransition(_itemIndex.row());
                    break;
                }
                }
            });


    updateTranslations();

    return view;
}

void ScreenplayTextManager::Implementation::setModelForView(BusinessLayer::AbstractModel* _model,
                                                            Ui::ScreenplayTextView* _view)
{
    constexpr int invalidIndex = -1;
    int viewIndex = invalidIndex;
    for (int index = 0; index < allViews.size(); ++index) {
        if (allViews[index].view == _view) {
            if (allViews[index].model == _model) {
                return;
            }

            viewIndex = index;
            break;
        }
    }

    //
    // Если модель была задана
    //
    if (viewIndex != invalidIndex && allViews[viewIndex].model != nullptr) {
        //
        // ... сохраняем параметры
        //
        saveModelAndViewSettings(allViews[viewIndex].model, _view);
        //
        // ... разрываем соединения
        //
        _view->disconnect(allViews[viewIndex].model);
        _view->disconnect(allViews[viewIndex].model->dictionariesModel());
    }

    //
    // Определяем новую модель
    //
    auto model = qobject_cast<BusinessLayer::ScreenplayTextModel*>(_model);
    _view->setModel(model);

    //
    // Обновляем связь представления с моделью
    //
    if (viewIndex != invalidIndex) {
        allViews[viewIndex].model = model;
    }
    //
    // Или сохраняем связь представления с моделью
    //
    else {
        allViews.append({ _view, model });
    }

    //
    // Если новая модель задана
    //
    if (model != nullptr) {
        //
        // ... загрузим параметры
        //
        loadModelAndViewSettings(model, _view);
        updateDictionaryItemsList(kSceneIntrosIndex, _view);
        //
        // ... настраиваем соединения
        //
        connect(model->dictionariesModel(),
                &BusinessLayer::ScreenplayDictionariesModel::sceneIntrosChanged, _view,
                [this, _view] {
                    if (_view->dictionariesView()->currentTypeIndex().row() == kSceneIntrosIndex) {
                        updateDictionaryItemsList(kSceneIntrosIndex, _view);
                    }
                });
        connect(model->dictionariesModel(),
                &BusinessLayer::ScreenplayDictionariesModel::sceneTimesChanged, _view,
                [this, _view] {
                    if (_view->dictionariesView()->currentTypeIndex().row() == kSceneTimesIndex) {
                        updateDictionaryItemsList(kSceneTimesIndex, _view);
                    }
                });
        connect(model->dictionariesModel(),
                &BusinessLayer::ScreenplayDictionariesModel::charactersExtensionsChanged, _view,
                [this, _view] {
                    if (_view->dictionariesView()->currentTypeIndex().row()
                        == kCharacterExtensionsIndex) {
                        updateDictionaryItemsList(kCharacterExtensionsIndex, _view);
                    }
                });
        connect(model->dictionariesModel(),
                &BusinessLayer::ScreenplayDictionariesModel::transitionsChanged, _view,
                [this, _view] {
                    if (_view->dictionariesView()->currentTypeIndex().row() == kTransitionIndex) {
                        updateDictionaryItemsList(kTransitionIndex, _view);
                    }
                });
    }
}

QPointer<BusinessLayer::ScreenplayTextModel> ScreenplayTextManager::Implementation::modelForView(
    Ui::ScreenplayTextView* _view) const
{
    for (auto& viewAndModel : allViews) {
        if (viewAndModel.view == _view) {
            return viewAndModel.model;
        }
    }
    return {};
}

void ScreenplayTextManager::Implementation::loadModelAndViewSettings(
    BusinessLayer::AbstractModel* _model, Ui::ScreenplayTextView* _view)
{
    const auto cursorPosition = settingsValue(cursorPositionFor(_model->document()), 0).toInt();
    _view->setCursorPosition(cursorPosition);
    const auto verticalScroll = settingsValue(verticalScrollFor(_model->document()), 0).toInt();
    _view->setverticalScroll(verticalScroll);

    _view->loadViewSettings();
}

void ScreenplayTextManager::Implementation::saveModelAndViewSettings(
    BusinessLayer::AbstractModel* _model, Ui::ScreenplayTextView* _view)
{
    setSettingsValue(cursorPositionFor(_model->document()), _view->cursorPosition());
    setSettingsValue(verticalScrollFor(_model->document()), _view->verticalScroll());

    _view->saveViewSettings();
}

void ScreenplayTextManager::Implementation::updateTranslations()
{
    dictionariesTypesModel->setStringList({
        tr("Scene intro"),
        tr("Scene time"),
        tr("Character extension"),
        tr("Transition"),
    });
}

void ScreenplayTextManager::Implementation::updateDictionaryItemsList(int _dictionaryType,
                                                                      Ui::ScreenplayTextView* _view)
{
    auto model = modelForView(_view);
    if (model == nullptr) {
        return;
    }

    switch (_dictionaryType) {
    case kSceneIntrosIndex: {
        dictionaryItemsModel->setStringList(model->dictionariesModel()->sceneIntros().toList());
        break;
    }

    case kSceneTimesIndex: {
        dictionaryItemsModel->setStringList(model->dictionariesModel()->sceneTimes().toList());
        break;
    }

    case kCharacterExtensionsIndex: {
        dictionaryItemsModel->setStringList(
            model->dictionariesModel()->characterExtensions().toList());
        break;
    }

    case kTransitionIndex: {
        dictionaryItemsModel->setStringList(model->dictionariesModel()->transitions().toList());
        break;
    }

    default: {
        dictionaryItemsModel->setStringList({});
        break;
    }
    }
}


// ****


ScreenplayTextManager::ScreenplayTextManager(QObject* _parent)
    : QObject(_parent)
    , d(new Implementation(this))
{
}

ScreenplayTextManager::~ScreenplayTextManager() = default;

QObject* ScreenplayTextManager::asQObject()
{
    return this;
}

Ui::IDocumentView* ScreenplayTextManager::view()
{
    return d->view;
}

Ui::IDocumentView* ScreenplayTextManager::view(BusinessLayer::AbstractModel* _model)
{
    if (d->view == nullptr) {
        d->view = d->createView(_model);

        //
        // Наружу даём сигналы только от первичного представления, только оно может
        // взаимодействовать с навигатором документа
        //
        connect(d->view, &Ui::ScreenplayTextView::currentModelIndexChanged, this,
                &ScreenplayTextManager::currentModelIndexChanged);
    } else {
        d->setModelForView(_model, d->view);
    }

    return d->view;
}

Ui::IDocumentView* ScreenplayTextManager::secondaryView()
{
    return d->secondaryView;
}

Ui::IDocumentView* ScreenplayTextManager::secondaryView(BusinessLayer::AbstractModel* _model)
{
    if (d->secondaryView == nullptr) {
        d->secondaryView = d->createView(_model);
    } else {
        d->setModelForView(_model, d->secondaryView);
    }

    return d->secondaryView;
}

Ui::IDocumentView* ScreenplayTextManager::createView(BusinessLayer::AbstractModel* _model)
{
    return d->createView(_model);
}

void ScreenplayTextManager::resetModels()
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.view.isNull()) {
            continue;
        }

        d->setModelForView(nullptr, viewAndModel.view);
    }
}

void ScreenplayTextManager::reconfigure(const QStringList& _changedSettingsKeys)
{
    d->view->reconfigure(_changedSettingsKeys);
}

void ScreenplayTextManager::bind(IDocumentManager* _manager)
{
    //
    // Т.к. навигатор соединяется только с главным инстансом редактора, проверяем создан ли он
    //
    if (d->view == nullptr) {
        return;
    }

    Q_ASSERT(_manager);

    const auto isConnectedFirstTime
        = connect(_manager->asQObject(), SIGNAL(currentModelIndexChanged(QModelIndex)), this,
                  SLOT(setCurrentModelIndex(QModelIndex)), Qt::UniqueConnection);
    connect(_manager->asQObject(), SIGNAL(pasteBeatNameToEditorRequested(QString)), this,
            SLOT(pasteBeatNameToEditor(QString)), Qt::UniqueConnection);

    //
    // Ставим в очередь событие нотификацию о смене текущей сцены,
    // чтобы навигатор отобразил её при первом открытии
    //
    if (isConnectedFirstTime) {
        QMetaObject::invokeMethod(
            this, [this] { emit currentModelIndexChanged(d->view->currentModelIndex()); },
            Qt::QueuedConnection);
    }
}

void ScreenplayTextManager::saveSettings()
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.model.isNull() || viewAndModel.view.isNull()) {
            continue;
        }

        d->saveModelAndViewSettings(viewAndModel.model, viewAndModel.view);
    }
}

void ScreenplayTextManager::setEditingMode(DocumentEditingMode _mode)
{
    for (auto& viewAndModel : d->allViews) {
        if (viewAndModel.view.isNull()) {
            continue;
        }

        viewAndModel.view->setEditingMode(_mode);
    }
}

bool ScreenplayTextManager::eventFilter(QObject* _watched, QEvent* _event)
{
    if (_event->type() == QEvent::LanguageChange && _watched == d->view) {
        d->updateTranslations();
    }

    return QObject::eventFilter(_watched, _event);
}

void ScreenplayTextManager::setCurrentModelIndex(const QModelIndex& _index)
{
    QSignalBlocker blocker(this);

    if (!_index.isValid()) {
        return;
    }

    d->view->setCurrentModelIndex(_index);
    d->view->setFocus();
}

void ScreenplayTextManager::pasteBeatNameToEditor(const QString& _name)
{
    d->view->insertText(_name);
}

} // namespace ManagementLayer
