#include <iostream>
#include <string>
#include <iomanip>
#include <json/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h> 
#include <tabulate/table.hpp>

bool iequals(const std::string& a, const std::string& b)
{
	return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char a, char b) { return tolower(a) == tolower(b); });
}

bool getYN()
{
	std::string choice_str;
	std::cin >> choice_str;

	if (iequals(choice_str, "y") || iequals(choice_str, "yes")) {
		return true;
	}

	return false;
}

bool is_digits(const std::string& str)
{
	return str.find_first_not_of("0123456789.") == std::string::npos;
}

static int callback(void* NotUsed, int argc, char** argv, char** azColName) {
	int i;
	for (i = 0; i < argc; i++) {
		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}
	printf("\n");
	return 0;
}

static int import_callback(void* data, int argc, char** argv, char** azColName);
static int fetch_callback(void* data, int argc, char** argv, char** azColName);

class Account
{
private:
	std::string password;

public:
	double balance = 0;
	std::string name;

	Account(std::string x, std::string y, double z = 0) {
		name = x;
		password = y;
		balance = z;
	}

	double getBalance()
	{
		return balance;
	}

	bool checkPassword(std::string pass)
	{
		return password.compare(pass) == 0;
	}

	void fetch(sqlite3* db)
	{
		std::string sql = "SELECT balance FROM accounts WHERE name='" + name + "'";
		char* zErrMsg = 0;
		int rc = sqlite3_exec(db, sql.c_str(), fetch_callback, (void*)this, &zErrMsg);

		if (rc != SQLITE_OK) {
			fprintf(stderr, "SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
	}

	void saveState(sqlite3* db)
	{
		std::string sql = "UPDATE accounts SET balance=" + std::to_string(balance) + " WHERE name='" + name + "'";
		char* zErrMsg = 0;
		int rc = sqlite3_exec(db, sql.c_str(), callback, 0, &zErrMsg);

		if (rc != SQLITE_OK) {
			fprintf(stderr, "SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
	}

	void showBalance()
	{
		std::cout << "Your balance is: " << std::setprecision(3) << std::fixed << balance << std::endl;
	}

	std::string deposite(double amount)
	{
		if (amount == 0)
		{
			return "Cannot deposite 0";
		}
		else if (amount < 0)
		{
			return "Cannot deposite a negative amount. For this purpose use withdraw.";
		}

		balance += amount;
		return "";
	}

	std::string withdraw(double amount)
	{
		if (amount == 0)
		{
			return "Cannot withdraw 0";
		}
		else if (amount < 0)
		{
			return "Cannot withdraw a negative amount. For this purpose use deposite.";
		}

		if (amount > balance)
		{
			std::cout << "You are about to go into debt.\nAre you sure you want to do that? [Y/n] ";

			if (!getYN()) {
				return "You chose not to go into debt.";
			}
		}

		balance -= amount;
		return "";
	}

	std::string transfer(Account* receiver, double amount)
	{
		if (amount > balance)
		{
			std::cout << "You are about to go into debt.\nAre you sure you want to do that? [Y/n] ";

			if (!getYN()) {
				return "You chose not to go into debt.";
			}
		}

		double* rec_balance = &receiver->balance;
		balance -= amount;
		*rec_balance += amount;

		return "";
	}
};

static int import_callback(void* data, int argc, char** argv, char** azColName) 
{
	std::vector<Account*>* accounts = (std::vector<Account*>*)data;
	accounts->emplace_back(new Account(argv[1], argv[2], std::stod(argv[3])));

	return 0;
}

static int fetch_callback(void* data, int argc, char** argv, char** azColName)
{
	Account* accounts = (Account*)data;
	accounts->balance = std::stod(argv[0]);

	return 0;
}

Account* findInAccounts(std::string name, std::vector<Account*>* accounts)
{
	for (Account* account : *accounts)
	{
		if (account->name == name)
		{
			return account;
		}
	}
	return NULL;
}

Account* searchByName(std::string name, std::vector<Account*>* accounts, sqlite3* db)
{
	Account* found = findInAccounts(name, accounts);

	if (found == NULL)
	{
		char* zErrMsg = 0;
		int rc;

		std::string sql = "SELECT * FROM accounts WHERE name='" + name + "'";

		rc = sqlite3_exec(db, sql.c_str(), import_callback, (void*)accounts, &zErrMsg);

		if (rc != SQLITE_OK) {
			fprintf(stderr, "SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}

		found = findInAccounts(name, accounts);

		if (found == NULL)
		{
			return NULL;
		}

		return found;
	}

	return found;
}

bool loggedIn(Account* account, std::vector<Account*> accounts, sqlite3* db)
{
	//std::cout << "This is the bank. If you don't like it, too bad." << std::endl;

	while (true)
	{
		std::cout << "\n\n";

		std::string choice_str = "";

		tabulate::Table options;
		options.add_row({ "", "Option", "Descripion" });
		options.add_row({ "1.", "Show balance", "Writes out your balance." });
		options.add_row({ "2.", "Deposite", "Add money to your account." });
		options.add_row({ "3.", "Withdraw", "Remove account from your account." });
		options.add_row({ "4.", "Transfer", "Transfer money from your account to someone else." });
		options.add_row({ "5.", "Log out", "Log out to be able to log in another account." });
		options.add_row({ "6.", "Exit", "Exit the banking system." });

		options.format()
			.font_style({ tabulate::FontStyle::bold });
		options.column(0).format()
			.font_color(tabulate::Color::magenta);
		options.column(1).format()
			.font_color(tabulate::Color::yellow);
		options[0].format()
			.font_color(tabulate::Color::blue);
		options[0][1].format()
			.font_color(tabulate::Color::blue);

		std::cout << options << std::endl;
		std::cout << "Choose between 1 and 6: ";
		std::cin >> choice_str;

		int choice;
		if (is_digits(choice_str))
		{
			choice = stoi(choice_str);
		}
		else {
			choice = 5;
		}

		std::string amount_str = "";
		std::string receiver_name;
		Account* receiver;
		std::string success;

		std::cout << std::endl;

		switch (choice) {
		case (1):
			account->fetch(db);
			account->showBalance();
			continue;
		case (2):
			std::cout << "How much do you want to deposit? ";
			std::cin >> amount_str;
			if (is_digits(amount_str))
			{
				double amount = stod(amount_str);
				account->fetch(db);
				success = account->deposite(amount);
				if (success != "")
				{
					std::cout << success << std::endl;
					continue;
				}
				account->showBalance();
				account->saveState(db);
			}
			else
			{
				std::cout << "\n Invalid input type" << std::endl;
			}
			continue;
		case (3):
			std::cout << "How much do you want to withdraw? ";
			std::cin >> amount_str;
			if (is_digits(amount_str))
			{
				double amount = stod(amount_str);
				account->fetch(db);
				success = account->withdraw(amount);
				if (success != "")
				{
					std::cout << success << std::endl;
					continue;
				}
				account->showBalance();
				account->saveState(db);
			}
			else
			{
				std::cout << "\n Invalid input type" << std::endl;
			}
			continue;
		case(4):
			std::cout << "Who do you want to transfer to? ";
			std::cin >> receiver_name;

			receiver = searchByName(receiver_name, &accounts, db);

			if (receiver == NULL)
			{
				std::cout << "The account you want to transfer to doesn't exist" << std::endl;
				continue;
			}

			std::cout << "How much do you want to transfer? ";
			std::cin >> amount_str;

			if (is_digits(amount_str))
			{
				double amount = stod(amount_str);
				account->fetch(db);
				receiver->fetch(db);
				success = account->transfer(receiver, amount);

				if (success != "")
				{
					std::cout << success << std::endl;
					continue;
				}
				account->showBalance();
				account->saveState(db);
				receiver->saveState(db);
			}
			else
			{
				std::cout << "\n Invalid input type" << std::endl;
			}

			continue;
		case(5):
			return true;
			break;
		case(6):
			std::cout << "You are sure you want to exit? [Y/n] ";

			if (!getYN()) {
				continue;
			}
			break;
		default:
			std::cout << "You lost, not valid option" << std::endl;
			continue;
		}

		break;
	}

	return false;
}

int main()
{
	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	std::vector<Account*> accounts;

	rc = sqlite3_open("accounts.db", &db);

	std::string sql = "CREATE TABLE IF NOT EXISTS 'accounts' ('id' INTEGER NOT NULL,'name' varchar(255) NOT NULL UNIQUE, 'password' varchar(255) NOT NULL,'balance' NUMERIC NOT NULL, PRIMARY KEY('id' AUTOINCREMENT));";
	rc = sqlite3_exec(db, sql.c_str(), callback, 0, &zErrMsg);

	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}

	while (true)
	{	
		tabulate::Table main;
		main.add_row({ "This is the bank. If you don't like it, too bad." });
		main.format()
			.font_align(tabulate::FontAlign::center)
			.font_style({ tabulate::FontStyle::bold})
			.font_color(tabulate::Color::yellow);

		std::cout << main << std::endl;
		std::cout << "What is your name? ";

		std::string name;
		std::cin >> name;

		Account* account = searchByName(name, &accounts, db);

		if (account == NULL) {
			std::cout << "\nNo account found with this name.\nDo you want to make a new one? [y/n] ";
			bool choice = getYN();

			if (choice)
			{
				while (true)
				{
					std::string password;
					std::string repassword;

					std::cout << "Password: ";
					std::cin >> password;
					std::cout << "Password again: ";
					std::cin >> repassword;

					if (password.compare(repassword) == 0)
					{
						accounts.emplace_back(new Account(name, password));
						std::cout << "\nAccount creation was successful\n" << std::endl;

						std::string sql = "INSERT INTO accounts (name, password, balance) VALUES ('" + name + "', '" + password + "', 0)";
						char* zErrMsg = 0;
						int rc = sqlite3_exec(db, sql.c_str(), callback, 0, &zErrMsg);

						if (rc != SQLITE_OK) {
							fprintf(stderr, "SQL error: %s\n", zErrMsg);
							sqlite3_free(zErrMsg);
						}

						break;
					}
				}
			}
		}
		else
		{
			std::string password;
			std::cout << "Password: ";
			std::cin >> password;

			if (account->checkPassword(password))
			{
				bool response = loggedIn(account, accounts, db);
				if (!response) 
				{
					break;
				}
			}
			else
			{
				std::cout << "\nPassword incorrect!" << std::endl;
			}
		}
	}

	sqlite3_close(db);
}