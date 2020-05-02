#include "algorithms.h"

Algorithms::Algorithms(Game* current_game, vector<Move>(*eachMove)(Chess::Player), bool (*movePiece)(Move))
{
	srand((unsigned)time(NULL));
	saves = new stack<Game>;
	this->current_game = current_game;
	game_copy = *current_game;
	this->eachMove = eachMove;
	this->movePiece = movePiece;
}

Algorithms::~Algorithms()
{
	current_game = nullptr;
	eachMove = nullptr;
	movePiece = nullptr;
	delete saves;
	delete MCTree;
}

bool Algorithms::doBestMove()
{
	return movePiece(bestMove);
}

void Algorithms::setMaxDepth(int depth)
{
	MAX_DEPTH = depth > 0 ? depth : MAX_DEPTH;
}

Chess::Player Algorithms::player(const bool maximizer)
{
	return maximizer ? Chess::WHITE_PLAYER : Chess::BLACK_PLAYER;
}

void Algorithms::save()
{
	saves->push(*current_game);
}

void Algorithms::load()
{
	*current_game = saves->top();
}

int Algorithms::minimaxSearch(bool maximizer, int depth, int alpha, int beta)
{
	if (depth == 0)
		gamesEvalauted = 0;
	totalEvaluated++;
	gamesEvalauted++;
	if (depth >= MAX_DEPTH) {
		return current_game->evaluate();
	}
	if (current_game->isCheckMate()) {
		Chess::Position king = current_game->findKing(current_game->getCurrentTurn());
		return current_game->evaluate() - current_game->pieceValue(king.iRow, king.iColumn);
	}
	vector<Move> validMoves = eachMove(player(maximizer));
	if (validMoves.size() == 0 || current_game->fiftyMoveRule()) {
		//no more moves and not a checkmate or fifty move rule causes stalemate
		current_game->setStaleMate();
		return 0;
	}
	save();
	if (maximizer) {
		//white's turn (maximzie the value)
		//for each move
		for (const auto& move : validMoves)
		{
			//cout << '(' << char('A' + move.present.iColumn) << move.present.iRow + 1
			//	<< '-' << char('A' + move.future.iColumn) << move.future.iRow + 1 << ')';
			movePiece(move);
			int searchValue = minimaxSearch(!maximizer, ++depth, alpha, beta);
			depth--;
			if (searchValue > alpha) {
				alpha = searchValue;
				if (depth == 0)
					bestMove = move;
			}
			//reset game to previous depth after each move
			load();
			if (alpha >= beta) {
				//prune the other moves
				saves->pop();
				return alpha;
			}
		}
		saves->pop();
		return alpha;
	}
	else {
		//black's turn (minimize the value)
		//for each move
		for (const auto& move : validMoves)
		{
			//cout << '(' << char('A' + move.present.iColumn) << move.present.iRow + 1
			//	<< '-' << char('A' + move.future.iColumn) << move.future.iRow + 1 << ')';
			movePiece(move);
			int searchValue = minimaxSearch(!maximizer, ++depth, alpha, beta);
			depth--;
			if (searchValue < beta) {
				beta = searchValue;
				if (depth == 0)
					bestMove = move;
			}
			//reset game to previous depth after each move
			load();
			if (alpha >= beta) {
				//prune the other moves
				saves->pop();
				return beta;
			}
		}
		saves->pop();
		return beta;
	}
}

int Algorithms::minimaxSearchTimed(bool maximizer, int depth, int alpha, int beta)
{
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	//function to record here
	int value = minimaxSearch(maximizer, depth, alpha, beta);
	//to here
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::chrono::duration<float> seconds = end - start;
	minimaxTimeElapsed = seconds.count();
	return value;
}

bool Algorithms::monteCarloTreeSearch(int seconds)
{
	nodesCreated = 0;
	save();
	//initialize tree if doesnt exists
	if (MCTree == nullptr) {//first call
		MCTree = new Node(*current_game, eachMove);
		totalNodesCreated++;
		nodesCreated++;
	}
	else {//set child node to root if exists else make new tree root
		//second call and after
		//check if last call was mcts implying mcts vs mcts
		if (*current_game == MCTree->bestChild()->data) {
			MCTree = MCTree->bestChild();
			MCTree->setRoot();
		}
		else {
			//get the child representing the opponent's move
			bool found = false;
			for (auto& child : (MCTree->bestChild())->children) {
				if (*current_game == child->data) {
					MCTree = child;
					MCTree->setRoot();
					found = true;
					break;
				}
			}
			if (!found) {
				delete MCTree;//the child does not exist it is safe to delete the whole tree
				MCTree = new Node(*current_game, eachMove);
				totalNodesCreated++;
				nodesCreated++;
			}
		}
	}
	//the player that the algo optimizes for
	bool white = MCTree->data.getCurrentTurn() == Chess::WHITE_PLAYER;
	//for x times build the tree
	auto start = std::chrono::high_resolution_clock::now();
	while (true)
	{
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - start;
		if (elapsed.count() >= seconds * 1000.0)//milliseconds
			break;
		//selection - select best child to explore including all valid moves
		//should prioritize promosing moves or unexplroed moves to balance
		Node* leaf = MCTree;
		//check if node is already terminal
		if (leaf->isTerminal()) {
			//mcts must be called where root node has children to choose from
			return false;
		}
		//if leaf has already explored all moves, pick the best result
		while (!leaf->hasPossibleChildren() && leaf->children.size() > 0) {
			leaf = leaf->bestUCTChild(white);
			*current_game = leaf->data;
		}
		// expand if the leaf is not terminal
		if (!leaf->isTerminal() && leaf->hasPossibleChildren()) {
			//expansion - add new child node to selected child
			Move randomMove = leaf->popRandomValidMove();
			movePiece(randomMove);
			leaf = leaf->addChild(new Node(*current_game, eachMove));
			*current_game = leaf->data;
			totalNodesCreated++;
			nodesCreated++;
		}
		//simulation - expand the child node randomly till finished
		int result = -2;
		bool playerIsWhite = leaf->data.getCurrentTurn() == 0 ? true : false;
		while (result == -2) {
			//end cases to stop simulation
			if (current_game->isCheckMate()) {
				if (current_game->getCurrentTurn() == 0)
					result = -1;//black wins
				else
					result = 1;//white wins
			}
			else {
				vector<Move> validMoves = eachMove(player(playerIsWhite));
				playerIsWhite = !playerIsWhite;
				if (validMoves.size() == 0 || current_game->fiftyMoveRule()) {
					//no more moves and not a checkmate or fifty move rule causes stalemate
					current_game->setStaleMate();
					result = 0;
				}
				//do rollout policy
				if (result == -2)
					movePiece(validMoves[rand() % validMoves.size()]);
			}
		}
		//backpropagation - back propagate result up the tree
		/*if (result != 0)
			cout << result << "\n";*/
		if (!leaf->backpropagate(result))
			cout << "***BACKPROPAGATE ERROR***" << "\n";
		//reset to original state
		load();
	}
	saves->pop();
	//assign best move the best move
	bestMove = MCTree->bestChild()->getLastMove();
	mctsEval = MCTree->bestChild()->data.evaluate();
	return true;
}

bool Algorithms::monteCarloTreeSearchTimed(int seconds)
{
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	//function to record here
	bool value = monteCarloTreeSearch(seconds);
	//to here
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::chrono::duration<float> time = end - start;
	mctsActualTime = time.count();
	return value;
}


