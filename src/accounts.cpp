
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <vector>
#include <algorithm>

#include <rdlib/StdFile.h>
#include <rdlib/strsup.h>
#include <rdlib/DateTime.h>
#include <rdlib/Regex.h>

typedef struct {
    ADateTime date;
    AString   account;
    int64_t   amount;
    AString   type;
    AString   desc;
    uint32_t  month;
    std::vector<AString> desclines;
} transaction_t;

typedef struct {
    AString  label;
    uint32_t count;
    int64_t  total;
    std::vector<size_t> list;
} transactionlist_t;

typedef struct {
    bool    allowpositive;
    bool    allownegative;
    bool    unused;
    AString type;
    AString desc;
    AString label;
} filter_t;

static bool sortbytotal(const transactionlist_t *a, const transactionlist_t *b)
{
    return (llabs(a->total) > llabs(b->total));
}

void GetColumns(const AString& str, std::vector<AString>& columns)
{
    uint_t i, n = str.CountColumns();

    for (i = 0; i < n; i++) {
        columns.push_back(str.Column(i).Words(0));
    }
}

int main(int argc, char *argv[])
{
    AStdFile fp;

    if ((argc > 1) && fp.open(argv[1])) {
        static const AString requiredcolumns[] = {
            "Date",
            "Account",
            "Amount",
            "Subcategory",
            "Memo",
        };
        static const AString  anyregex      = ParseGlob("*");
        static const AString  directdeposit = ParseGlob("directdep");
        static const AString  directdebit   = ParseGlob("(directdebit)|(repeatpmt)");
        static const AString  payment       = ParseGlob("payment");
        static const AString  fundstransfer = ParseGlob("ft");
        static const AString  cash          = ParseGlob("cash");
        static const filter_t filters[] = {
            {false,  true,  false, anyregex,              anyregex,                            "Outgoings"},

            { true,  true,  false, directdeposit,         ParseGlob("bbc monthly*"),           "Salary"},
            { true,  true,  false, directdeposit,         ParseGlob("mt safeline*"),           "Salary"},
            { true,  true,  false, directdeposit,         ParseGlob("*edmondson*"),            "Gail"},
            { true,  true,  true,  directdeposit,         ParseGlob("*bbc*"),                  "BBC Expenses"},
            { true,  true,  true,  directdeposit,         ParseGlob("*"),                      ""},

            { true,  true,  false, directdebit,           ParseGlob("*"),                      "Bills"},
            { true,  true,  false, directdebit,           ParseGlob("*"),                      ""},

            { true,  true,  false, payment,               ParseGlob("nowtv.com*"),             "Bills"},
            { true,  true,  false, payment,               ParseGlob("nowtv.com*"),             "NOW TV"},
            { true,  true,  false, payment,               ParseGlob("google*"),                "Google"},

            { true,  true,  false, payment,               ParseGlob("*peel media*"),           "Bills"},

            { true,  true,  false, fundstransfer,         ParseGlob("*83111512*"),             "Mortgate Transfers"},

            { true,  true,  false, cash,                  ParseGlob("*"),                      "Cash"},

            { true,  true,  false, payment,               ParseGlob("*amazon*"),               "Amazon"},
            { true,  true,  false, payment,               ParseGlob("*paypal*"),               "PayPal"},
            { true,  true,  false, payment,               ParseGlob("*tesco*"),                "Tesco"},
            { true,  true,  false, payment,               ParseGlob("*asda*"),                 "Asda"},
            { true,  true,  false, payment,               ParseGlob("*sainsburys*"),           "Sainsburys"},
            { true,  true,  false, payment,               ParseGlob("*santaspizza*"),          "Santas"},
            { true,  true,  false, payment,               ParseGlob("*mcdonalds*"),            "McDonalds"},
            { true,  true,  false, payment,               ParseGlob("*kwik fit*"),             "Kwik Fit"},

            {false,  true,  true,  ParseGlob("*"),        ParseGlob("*"),                      "Other Payments"},
            { true, false,  true,  ParseGlob("*"),        ParseGlob("*"),                      "Other Receipts"},
        };
        std::vector<AString>       columnlist;
        std::map<AString,size_t>   columnmap;
        std::vector<transaction_t> transactions;
        std::vector<size_t>        months;
        AString  line;
        uint32_t month = 0;
        uint32_t ln = 0;
        size_t   i;

        while (line.ReadLn(fp) >= 0) {
            std::vector<AString> columns;
            size_t i;

            GetColumns(line, columns);

            if (ln == 0) {
                columnlist = columns;

                for (i = 0; i < columnlist.size(); i++) {
                    columnmap[columnlist[i]] = i;
                }

                for (i = 0; i < NUMBEROF(requiredcolumns); i++) {
                    if (columnmap.find(requiredcolumns[i]) == columnmap.end()) {
                        fprintf(stderr, "Required column '%s' missing\n", requiredcolumns[i].str());
                        break;
                    }
                }

                if (i < NUMBEROF(requiredcolumns)) {
                    break;
                }
            }
            else {
                transaction_t trans;
                size_t n = 0;

                trans.date.StrToDate(columns[columnmap[requiredcolumns[n++]]]);
                trans.account = columns[columnmap[requiredcolumns[n++]]];
                trans.amount  = (int64_t)((double)columns[columnmap[requiredcolumns[n++]]] * 100.0);
                trans.type    = columns[columnmap[requiredcolumns[n++]]];
                trans.desc    = columns[columnmap[requiredcolumns[n++]]];
                int p, p1 = 0;

                while ((p = trans.desc.Pos("   ", p1)) >= 0) {
                    trans.desclines.push_back(trans.desc.Mid(p1, p).Words(0));
                    p1 = p + 3;
                }
                trans.desclines.push_back(trans.desc.Mid(p1).Words(0));

                transactions.insert(transactions.begin(), trans);
            }

            ln++;
        }

        fp.close();

        months.push_back(0);

        for (i = 0; i < transactions.size(); i++) {
            transaction_t& trans = transactions[i];

            if (/*(trans.date.GetMonth() != transactions[months[months.size() - 1]].date.GetMonth()) &&*/
                (trans.amount > 0) &&
                ((trans.desc.PosNoCase("bbc monthly") >= 0) ||
                 (trans.desc.PosNoCase("mt safeline") >= 0))) {
                //printf("New pay point at %s, transaction %u\n", trans.date.DateToStr().str(), (uint_t)i);
                month++;
                months.push_back(i);
            }

            trans.month = month;
        }

        if ((months.size() > 0) && (transactions.size() > months[months.size()-1])) {
            months.push_back(transactions.size());
        }

        printf("%u transactions found\n", (uint_t)transactions.size());

        std::map<AString,transactionlist_t> transtypes;

        {
            size_t j;

            for (i = 0; i < transactions.size(); i++) {
                std::map<AString, bool> addedtolabel;
                const transaction_t& trans = transactions[i];
                uint_t count = 0;

                for (j = 0; j < NUMBEROF(filters); j++) {
                    const filter_t& filter = filters[j];
                    AString label = filter.label.Valid() ? filter.label : trans.desclines[0];

                    if (((filter.allowpositive && (trans.amount > 0)) ||
                         (filter.allownegative && (trans.amount < 0))) &&
                        MatchGlob(trans.type, filter.type) &&
                        MatchGlob(trans.desc, filter.desc) &&
                        !addedtolabel[label] &&
                        (!count || !filter.unused)) {
                        transactionlist_t& list = transtypes[label];

                        list.label = label;
                        list.count++;
                        list.total += trans.amount;
                        list.list.push_back(i);

                        addedtolabel[label] = true;

                        count++;
                    }
                }

                if (count == 0) {
                    fprintf(stderr, "Warning: transaction at %s, '%s' as not accumulated\n",
                            trans.date.DateFormat("%D/%M/%Y").str(),
                            trans.desclines[0].str());
                }
            }
        }

        {
            std::vector<const transactionlist_t *> list;
            std::map<AString, transactionlist_t>::iterator it;
            size_t j, k;

            for (it = transtypes.begin(); it != transtypes.end(); ++it) {
                list.push_back(&it->second);
            }

            std::sort(list.begin(), list.end(), sortbytotal);

#if 0
            for (i = 0; i < list.size(); i++) {
                const transactionlist_t& labellist = *list[i];

                if (labellist.count > 0) {
                    printf("Label '%s' amount %0.2lf count %u average %0.2lf\n",
                           labellist.label.str(),
                           (double)labellist.total * .01,
                           labellist.count,
                           (double)labellist.total * .01 / (double)labellist.count);
                }
            }
#endif

            if (fp.open("results.csv", "w")) {
                fp.printf("Month");

                for (i = 0; i < list.size(); i++) {
                    const transactionlist_t& labellist = *list[i];

                    if (labellist.count > 0) {
                        fp.printf(",\"%s\"", labellist.label.str());
                    }
                }

                fp.printf("\n");

                for (j = 0; j < (months.size() - 1); j++) {
                    const size_t firsttrans = months[j];
                    const size_t lasttrans  = months[j + 1];

                    fp.printf("%s", transactions[firsttrans].date.DateFormat("%Y-%M-%D").str());

                    for (i = 0; i < list.size(); i++) {
                        const transactionlist_t& labellist = *list[i];

                        if (labellist.count > 0) {
                            uint32_t count = 0;
                            int64_t  total = 0;

                            for (k = 0; k < labellist.list.size(); k++) {
                                size_t tran = labellist.list[k];

                                if ((tran >= firsttrans) && (tran < lasttrans)) {
                                    count++;
                                    total += transactions[tran].amount;
                                }
                            }

                            fp.printf(",%0.2lf", (double)total * .01);
                        }
                    }

                    fp.printf("\n");
                }

                fp.close();
            }

            if (fp.open("results.dat", "w")) {
                const ADateTime startdt = ADateTime().StrToDate("1/4/2016");
                ADateTime monthdt;
                uint32_t lastmonth = ~0;
                int32_t  total = 0, monthtotal = 0;

                for (i = 0; i < transactions.size(); i++) {
                    const transaction_t& trans = transactions[i];

                    if (trans.month != lastmonth) {
                        fp.printf("\n");
                        monthdt   = trans.date;
                        lastmonth = trans.month;
                        monthtotal = 0;
                    }

                    total += trans.amount;
                    monthtotal += trans.amount;

                    if (monthdt >= startdt) {
                        fp.printf("%s %u %0.2f %0.2f %0.2f\n",
                                  monthdt.DateFormat("%D/%M/%Y").str(),
                                  trans.date.GetDays() - monthdt.GetDays(),
                                  (double)trans.amount * .01,
                                  (double)total * .01,
                                  (double)monthtotal * .01);
                    }
                }

                fp.close();
            }
        }
    }

    return 0;
}
