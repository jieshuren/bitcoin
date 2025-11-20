// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/overviewpage.h>
#include <qt/forms/ui_overviewpage.h>

#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/transactionfilterproxy.h>
#include <qt/transactionoverviewwidget.h>
#include <qt/transactiontablemodel.h>
#include <qt/walletmodel.h>

#include <interfaces/node.h>
#include <key.h>
#include <chainparams.h>
#include <rpc/server.h>
#include <rpc/client.h>
#include <util/strencodings.h>
#include <interfaces/wallet.h>

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QDateTime>
#include <QPainter>
#include <QStatusTipEvent>
#include <QPushButton>
#include <QSpinBox>

#include <algorithm>
#include <map>
#include <thread>
#include <chrono>
#include <atomic>

#define DECORATION_SIZE 54
#define NUM_ITEMS 5

Q_DECLARE_METATYPE(interfaces::WalletBalances)

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    explicit TxViewDelegate(const PlatformStyle* _platformStyle, QObject* parent = nullptr)
        : QAbstractItemDelegate(parent), platformStyle(_platformStyle)
    {
        connect(this, &TxViewDelegate::width_changed, this, &TxViewDelegate::sizeHintChanged);
    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const override
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(TransactionTableModel::RawDecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon = platformStyle->SingleColorIcon(icon);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QBrush>())
        {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        QRect boundingRect;
        painter->drawText(addressRect, Qt::AlignLeft | Qt::AlignVCenter, address, &boundingRect);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true, BitcoinUnits::SeparatorStyle::ALWAYS);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }

        QRect amount_bounding_rect;
        painter->drawText(amountRect, Qt::AlignRight | Qt::AlignVCenter, amountText, &amount_bounding_rect);

        painter->setPen(option.palette.color(QPalette::Text));
        QRect date_bounding_rect;
        painter->drawText(amountRect, Qt::AlignLeft | Qt::AlignVCenter, GUIUtil::dateTimeStr(date), &date_bounding_rect);

        // 0.4*date_bounding_rect.width() is used to visually distinguish a date from an amount.
        const int minimum_width = 1.4 * date_bounding_rect.width() + amount_bounding_rect.width();
        const auto search = m_minimum_width.find(index.row());
        if (search == m_minimum_width.end() || search->second != minimum_width) {
            m_minimum_width[index.row()] = minimum_width;
            Q_EMIT width_changed(index);
        }

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const auto search = m_minimum_width.find(index.row());
        const int minimum_text_width = search == m_minimum_width.end() ? 0 : search->second;
        return {DECORATION_SIZE + 8 + minimum_text_width, DECORATION_SIZE};
    }

    BitcoinUnit unit{BitcoinUnit::HBC};

Q_SIGNALS:
    //! An intermediate signal for emitting from the `paint() const` member function.
    void width_changed(const QModelIndex& index) const;

private:
    const PlatformStyle* platformStyle;
    mutable std::map<int, int> m_minimum_width;
};

#include <qt/overviewpage.moc>

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    m_platform_style{platformStyle},
    txdelegate(new TxViewDelegate(platformStyle, this))
{
    ui->setupUi(this);

    // use a SingleColorIcon for the "out of sync warning" icon
    QIcon icon = m_platform_style->SingleColorIcon(QStringLiteral(":/icons/warning"));
    ui->labelTransactionsStatus->setIcon(icon);
    ui->labelWalletStatus->setIcon(icon);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, &TransactionOverviewWidget::clicked, this, &OverviewPage::handleTransactionClicked);

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
    connect(ui->labelWalletStatus, &QPushButton::clicked, this, &OverviewPage::outOfSyncWarningClicked);
    connect(ui->labelTransactionsStatus, &QPushButton::clicked, this, &OverviewPage::outOfSyncWarningClicked);
    
    // Mining button connections (initialize with disabled state)
    connect(ui->startMiningButton, &QPushButton::clicked, this, &OverviewPage::startMining);
    connect(ui->stopMiningButton, &QPushButton::clicked, this, &OverviewPage::stopMining);
    connect(ui->numThreadsBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), 
            this, &OverviewPage::updateMiningStatus);
    
    // Initially disable mining buttons if no wallet is available
    ui->startMiningButton->setEnabled(false);
    ui->stopMiningButton->setEnabled(false);
    ui->numThreadsBox->setEnabled(false);
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

void OverviewPage::setPrivacy(bool privacy)
{
    m_privacy = privacy;
    clientModel->getOptionsModel()->setOption(OptionsModel::OptionID::MaskValues, privacy);
    const auto& balances = walletModel->getCachedBalance();
    if (balances.balance != -1) {
        setBalance(balances);
    }

    ui->listTransactions->setVisible(!m_privacy);

    const QString status_tip = m_privacy ? tr("Privacy mode activated for the Overview tab. To unmask the values, uncheck Settings->Mask values.") : "";
    setStatusTip(status_tip);
    QStatusTipEvent event(status_tip);
    QApplication::sendEvent(this, &event);
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::setBalance(const interfaces::WalletBalances& balances)
{
    BitcoinUnit unit = walletModel->getOptionsModel()->getDisplayUnit();
    ui->labelBalance->setText(BitcoinUnits::formatWithPrivacy(unit, balances.balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
    ui->labelUnconfirmed->setText(BitcoinUnits::formatWithPrivacy(unit, balances.unconfirmed_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
    ui->labelImmature->setText(BitcoinUnits::formatWithPrivacy(unit, balances.immature_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
    ui->labelTotal->setText(BitcoinUnits::formatWithPrivacy(unit, balances.balance + balances.unconfirmed_balance + balances.immature_balance, BitcoinUnits::SeparatorStyle::ALWAYS, m_privacy));
    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = balances.immature_balance != 0;

    ui->labelImmature->setVisible(showImmature);
    ui->labelImmatureText->setVisible(showImmature);
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model) {
        // Show warning, for example if this is a prerelease version
        connect(model, &ClientModel::alertsChanged, this, &OverviewPage::updateAlerts);
        updateAlerts(model->getStatusBarWarnings());

        connect(model->getOptionsModel(), &OptionsModel::fontForMoneyChanged, this, &OverviewPage::setMonospacedFont);
        setMonospacedFont(clientModel->getOptionsModel()->getFontForMoney());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter.reset(new TransactionFilterProxy());
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter.get());
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        connect(filter.get(), &TransactionFilterProxy::rowsInserted, this, &OverviewPage::LimitTransactionRows);
        connect(filter.get(), &TransactionFilterProxy::rowsRemoved, this, &OverviewPage::LimitTransactionRows);
        connect(filter.get(), &TransactionFilterProxy::rowsMoved, this, &OverviewPage::LimitTransactionRows);
        LimitTransactionRows();
        // Keep up to date with wallet
        setBalance(model->getCachedBalance());
        connect(model, &WalletModel::balanceChanged, this, &OverviewPage::setBalance);

        connect(model->getOptionsModel(), &OptionsModel::displayUnitChanged, this, &OverviewPage::updateDisplayUnit);
    }

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
    
    // Enable mining buttons if we have a wallet (mining only works in regtest mode from UI)
    if (model) {
        ui->startMiningButton->setEnabled(true);
        ui->numThreadsBox->setEnabled(true);
    }
}

void OverviewPage::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::PaletteChange) {
        QIcon icon = m_platform_style->SingleColorIcon(QStringLiteral(":/icons/warning"));
        ui->labelTransactionsStatus->setIcon(icon);
        ui->labelWalletStatus->setIcon(icon);
    }

    QWidget::changeEvent(e);
}

// Only show most recent NUM_ITEMS rows
void OverviewPage::LimitTransactionRows()
{
    if (filter && ui->listTransactions && ui->listTransactions->model() && filter.get() == ui->listTransactions->model()) {
        for (int i = 0; i < filter->rowCount(); ++i) {
            ui->listTransactions->setRowHidden(i, i >= NUM_ITEMS);
        }
    }
}

void OverviewPage::updateDisplayUnit()
{
    if (walletModel && walletModel->getOptionsModel()) {
        const auto& balances = walletModel->getCachedBalance();
        if (balances.balance != -1) {
            setBalance(balances);
        }

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}

void OverviewPage::setMonospacedFont(const QFont& f)
{
    ui->labelBalance->setFont(f);
    ui->labelUnconfirmed->setFont(f);
    ui->labelImmature->setFont(f);
    ui->labelTotal->setFont(f);
}
void OverviewPage::startMining()
{
    if (m_mining_active) return;
    
    // Get mining address from wallet
    if (!walletModel) {
        ui->labelMiningStatus->setText(tr("No wallet available for mining"));
        return;
    }
    
    // Get mining address - for now we'll use a default approach
    QString address;
    // In a real implementation, you would get an address from the wallet
    // For now, we'll just use a placeholder to allow compilation
    address = "SimulatedMiningAddress";
    
    if (address.isEmpty()) {
        ui->labelMiningStatus->setText(tr("Failed to get mining address"));
        return;
    }
    
    m_mining_address = address;
    m_num_mining_threads = ui->numThreadsBox->value();
    m_should_stop_mining = false;
    m_mining_active = true;
    
    ui->labelMiningStatus->setText(tr("Mining..."));
    ui->startMiningButton->setEnabled(false);
    ui->stopMiningButton->setEnabled(true);
    ui->numThreadsBox->setEnabled(false);
    
    // Start mining in a separate thread - for regtest mode, we'll generate blocks via RPC
    m_mining_thread = std::thread([this]() {
        int blocks_mined = 0;
        
        while (!m_should_stop_mining) {
            try {
                // Get the node interface
                if (!clientModel) {
                    QMetaObject::invokeMethod(this, [this]() {
                        ui->labelMiningStatus->setText(tr("No client model"));
                    });
                    break;
                }
                
                // For now, we'll use a simple block generation method without specific address
                // In regtest, we can use the generate method which mines blocks to a wallet address
                std::vector<std::string> args = {
                    "1" // mine 1 block
                };

                // Execute the RPC call to mine a block in regtest mode
                try {
                    JSONRPCRequest req;
                    req.params = RPCConvertValues("generate", args);
                    req.strMethod = "generate";
                    
                    // Use the node's RPC interface to generate a block
                    // Using empty string for URI parameter
                    UniValue result = clientModel->node().executeRpc(req.strMethod, req.params, "");
                    
                    blocks_mined++;
                    
                    QMetaObject::invokeMethod(this, [this, blocks_mined]() {
                        ui->labelMiningStatus->setText(tr("Mining... Mined %1 blocks").arg(blocks_mined));
                    });
                    
                    // In regtest mode, blocks mine instantly, so we can add a small delay
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                } catch (...) {
                    QMetaObject::invokeMethod(this, [this]() {
                        ui->labelMiningStatus->setText(tr("Error mining block"));
                    });
                    break;
                }
                
            } catch (...) {
                // Handle any errors
                QMetaObject::invokeMethod(this, [this]() {
                    ui->labelMiningStatus->setText(tr("Mining error"));
                });
                break;
            }
        }
        
        m_mining_active = false;
    });
}

void OverviewPage::stopMining()
{
    if (!m_mining_active) return;
    
    m_should_stop_mining = true;
    if (m_mining_thread.joinable()) {
        m_mining_thread.join();
    }
    
    m_mining_active = false;
    ui->labelMiningStatus->setText(tr("Mining stopped"));
    ui->startMiningButton->setEnabled(true);
    ui->stopMiningButton->setEnabled(false);
    ui->numThreadsBox->setEnabled(true);
}

void OverviewPage::updateMiningStatus()
{
    if (m_mining_active) {
        m_num_mining_threads = ui->numThreadsBox->value();
    }
}