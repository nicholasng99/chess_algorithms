#pragma once
#include "chess.h"
#include <algorithm>
#include <stack> 

class Algorithms
{
public:
	struct Move {
		Chess::Position present;
		Chess::Position future;
		Chess::EnPassant S_enPassant;
		Chess::Castling S_castling;
		Chess::Promotion S_promotion;
	};

	//for mcts
	struct Node {
		//node vars
		int depth;
		Node* parent = nullptr;
		//explored moves are represented in children
		vector<Node*> children;
		//unexplored moves are represented in validMoves
		vector<Move> validMoves;
		//game data
		Game data;
		//stats
		int visitCount;
		int gamesWon;
		int gamesLost;//to calculate games drawn
		//functions
		//node constructor
		Node(Game& game, vector<Move>(*eachMove)(Chess::Player), Node* parent = nullptr) {
			data = game;
			validMoves = eachMove(game.getCurrentTurn() == 0 ? Chess::WHITE_PLAYER : Chess::BLACK_PLAYER);
			if (parent == nullptr)
				depth = 0;
			else {
				this->parent = parent;
				depth = parent->depth + 1;
			}
			visitCount = 0;
			gamesWon = 0;
			gamesLost = 0;
		}
		~Node() {
			parent = nullptr;
			delete parent;
			for (auto& child : children) {
				delete child;
			}
		}

		Node& operator=(Node& n) {
			depth = n.depth;
			parent = n.parent;
			children = n.children;
			data = n.data;
			validMoves = n.validMoves;
			visitCount = n.visitCount;
			gamesWon = n.gamesWon;
			gamesLost = n.gamesLost;
			return *this;
		}

		Node* addChild(Node* node) {
			children.push_back(node);
			return children[children.size() - 1];
		}
		bool isLeaf() {
			return children.size() == 0;
		}
		bool isTerminal() {
			//check mate
			if (data.isCheckMate())
				return true;
			//stalemate
			if (data.fiftyMoveRule())
				return true;
			if (validMoves.size() == 0 && children.size() == 0)
				return true;
			//not terminal
			return false;
		}

		//-1 for black win 0 for stalemate 1 for white win, -2 for not ended
		int result() {
			if (data.isCheckMate()) {
				//checkmate on white's turn aka black win
				if (data.getCurrentTurn() == 0)
					return -1;
				else//vice versa
					return 1;
			}
			if (data.fiftyMoveRule() || validMoves.size() == 0)
				return 0;
			return -2;
		}

		void setRoot() {
			if (parent != nullptr) {
				bool unlinked = false;
				for (auto& child : parent->children) {
					if (child == this) {//find the child pointer to this node
						child = nullptr;
						unlinked = true;
					}
				}
				if (unlinked) {
					delete parent;//delete everything in parent except this instance
					parent = nullptr;
					depth = 0;
				}
			}
		}

		Move getLastMove() {
			if (depth != 0) {
				Move move;
				data.parseMove(data.getLastMove(), &move.present, &move.future);
				return move;
			}
			Chess::Position pos{ -1,-1 };
			return Move{ pos,pos };
		}
		//recursive backpropagation in terms of white=true
		bool backpropagate(int result, bool white = true) {
			if (result < -1 || result > 1)
				return false;
			visitCount++;
			if (!white)
				result *= -1;
			gamesWon += result < 0 ? 0 : result;
			gamesLost -= result > 0 ? 0 : result;
			if (depth != 0)
				return parent->backpropagate(result);
			else
				return true;
		}

		const float winRate() const {
			if (visitCount == 0)
				return 0.0f;
			return gamesWon / visitCount;
		}
		const float loseRate() const {
			if (visitCount == 0)
				return 0.0f;
			return gamesLost / visitCount;
		}

		const float drawRate() const {
			if (visitCount == 0)
				return 0.0f;
			return (visitCount - gamesWon - gamesLost) / visitCount;
		}

		//returns the best winrate child node, nullptr if no child
		Node* bestChild() {
			if (children.size() == 0)
				return nullptr;
			Node* best = children[0];
			for (auto& child : children) {
				if (child->winRate() > best->winRate())
					best = child;
			}
			return best;
		}

		float UCT() {
			if (depth == 0)
				return 0.0f;
			//ucb1 formula
			return winRate() + sqrt(2) * sqrt(log(parent->visitCount) / visitCount);
		}

		// returns best UCT  node of all explored leaf nodes, nullptr if no child
		Node* bestUCTChild() {
			if (isLeaf())
				return this;
			Node* best = children[0];
			for (auto& child : children) {
				if (child->UCT() > best->UCT())
					best = child;
			}
			return best;
		}

		//returns and erases the valid move
		Move popRandomValidMove() {
			int randomIndex = rand() % validMoves.size();
			Move randomMove = validMoves[randomIndex];
			validMoves.erase(validMoves.begin() + randomIndex);
			return randomMove;
		}

		//if there are possible children to consider exploring
		bool hasPossibleChildren() {
			return validMoves.size() > 0;
		}
	};

	const int MAX_DEPTH = 5;
	const int TIME_LIMIT = 5;
	Move bestMove;
	//game pointer
	Game* current_game = nullptr;
	//each move function pointer
	vector<Move>(*eachMove)(Chess::Player) { nullptr };
	//move piece function pointer
	bool (*movePiece)(Move) { nullptr };

	Algorithms(Game* current_game, vector<Move>(*eachMove)(Chess::Player), bool (*movePiece)(Move));
	~Algorithms();

	bool doBestMove();

	Chess::Player player(const bool maximizer);

	void save();

	void load();

	int minimaxSearch(bool maximizer, int depth = 0, int alpha = std::numeric_limits<int>::min(), int beta = std::numeric_limits<int>::max());

	//returns true for success, false for terminal
	bool monteCarloTreeSearch(bool white);

private:
	//original game copy to backup current state
	Game game_copy = NULL;
	//game copies to save and revert to at each depth
	stack<Game>* saves;
	Node* MCTree = nullptr;
};